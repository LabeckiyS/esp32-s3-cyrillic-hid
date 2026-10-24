/*
 * ESP32-S3 USB-HID keyboard + mouse dongle — shared logic for all boards.
 *
 * Plug into the TARGET computer; it enumerates as a real USB keyboard + mouse. At
 * boot it serves a WiFi access point (http://192.168.4.1); the button toggles to a
 * BLE control service (web/control.html). Text/keys/mouse arrive over whichever
 * radio is active.
 *
 * ALL board-specific hardware (pins, screen driver, display) lives in boards/<board>.h,
 * so a change to one board can't affect another. Select the board with -DBOARD=n
 * (1 LilyGo, 2 Geek, 3 AtomS3U) or the #define below.
 *
 * Board options: USB CDC On Boot Enabled, USB Mode USB-OTG (TinyUSB),
 *   Partition 16M (3MB APP) [AtomS3U: 8M], Flash 16MB [AtomS3U: 8MB].
 * Libraries: Adafruit GFX, Adafruit ST7735 and ST7789 (pulled in by the board header).
 */

#include "USB.h"
#include "USBHIDKeyboard.h"
#include "USBHIDMouse.h"
#include <WiFi.h>
#include <WebServer.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <BLESecurity.h>
#include <esp_mac.h>
#include <esp_random.h>

// ===================== BOARD SELECT =====================
#define BOARD_LILYGO_TDONGLE_S3 1
#define BOARD_WAVESHARE_S3_GEEK 2
#define BOARD_M5STACK_ATOMS3U   3
#ifndef BOARD
#define BOARD BOARD_WAVESHARE_S3_GEEK   // default; CI/arduino-cli pass -DBOARD=n
#endif
#if   BOARD == BOARD_LILYGO_TDONGLE_S3
  #include "boards/lilygo.h"
#elif BOARD == BOARD_WAVESHARE_S3_GEEK
  #include "boards/geek.h"
#elif BOARD == BOARD_M5STACK_ATOMS3U
  #include "boards/atoms3u.h"
#else
  #error "Unknown BOARD"
#endif

// 0 = open BLE control (reliable); 1 = require an on-screen pairing PIN (needs a display)
#define REQUIRE_BLE_PAIRING 0
#if REQUIRE_BLE_PAIRING && !HAS_DISPLAY
  #error "REQUIRE_BLE_PAIRING needs a display to show the PIN; this board has none."
#endif

// RGB565 title colours (so shared code needs no display library)
#define DC_CYAN    0x07FF
#define DC_GREEN   0x07E0
#define DC_MAGENTA 0xF81F

// ---- Nordic UART Service UUIDs (match web/control.html) ----
#define SERVICE_UUID "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHAR_RX_UUID "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHAR_TX_UUID "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

USBHIDKeyboard Keyboard;
USBHIDMouse Mouse;

// A USB keyboard report must remain visible for several host polling cycles.
// Keyboard.write() sends press/release back-to-back, which caused occasional
// missing letters on some PCs.  Keep both states for 8 ms for reliable typing.
constexpr uint16_t HID_KEY_DOWN_MS = 8;
constexpr uint16_t HID_KEY_GAP_MS = 8;
WebServer server(80);
BLEAdvertising *advertising = nullptr;

String apSsid, apPass;
volatile uint32_t g_pairPin = 0;
volatile bool g_showPin = false;
volatile bool needRedraw = false;
int radioMode = 1;            // 0 = WiFi AP, 1 = BLE (BLE is the default)
volatile bool bleConnected = false;
volatile int wifiClients = 0;
bool wifiApUp = false;
volatile bool bleAdvOn = false;
uint8_t pendingUtf8Lead = 0;
bool russianTextContext = false;

const char INDEX_HTML[] PROGMEM = R"HTML(
<!DOCTYPE html><html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>S3 Keyboard</title><style>
body{font-family:system-ui,sans-serif;max-width:640px;margin:24px auto;padding:0 16px;background:#111;color:#eee}
h1{font-size:1.2rem}
textarea{width:100%;height:150px;font-size:16px;padding:10px;box-sizing:border-box;background:#1c1c1c;color:#eee;border:1px solid #444;border-radius:8px}
.row{display:flex;gap:8px;flex-wrap:wrap;margin-top:10px;align-items:center}
button{font-size:15px;padding:10px 16px;border:0;border-radius:8px;cursor:pointer;background:#3b82f6;color:#fff}
button.alt{background:#374151}label{display:flex;align-items:center;gap:6px;font-size:14px}
#log{margin-top:12px;font-size:13px;color:#9ca3af}
h2{font-size:1rem;color:#9ca3af;margin-top:22px}
#pad{touch-action:none;height:170px;margin-top:10px;background:#1c1c1c;border:1px dashed #555;border-radius:10px;display:flex;align-items:center;justify-content:center;color:#666;user-select:none;font-size:14px}
</style></head><body>
<h1>ESP32-S3 — Remote Keyboard (WiFi)</h1>
<textarea id="txt" placeholder="Type text to send to the target computer..."></textarea>
<div class="row"><button onclick="send()">Send</button>
<label><input type="checkbox" id="enter" checked> Press Enter after</label></div>
<div class="row">
<button class="alt" onclick="key('ENTER')">Enter</button>
<button class="alt" onclick="key('TAB')">Tab</button>
<button class="alt" onclick="key('ESC')">Esc</button>
<button class="alt" onclick="key('WIN')">Win/Cmd</button>
<button class="alt" onclick="key('CTRL_ALT_DEL')">Ctrl+Alt+Del</button></div>
<h2>Mouse</h2>
<div id="pad">drag to move the pointer &#8226; tap to left-click</div>
<div class="row"><label>Sensitivity <input type="range" id="sens" min="0.3" max="5" step="0.1" value="1.6"> <span id="sensv">1.6</span>x</label></div>
<div class="row">
<button class="alt" onclick="mclick(1)">Left</button>
<button class="alt" onclick="mclick(2)">Right</button>
<button class="alt" onclick="mclick(4)">Middle</button>
<button class="alt" onclick="mscroll(-3)">Scroll up</button>
<button class="alt" onclick="mscroll(3)">Scroll down</button></div>
<div id="log"></div><script>
async function post(p,b){const r=await fetch(p,{method:'POST',headers:{'Content-Type':'text/plain'},body:b});
document.getElementById('log').textContent=r.ok?'sent':'error '+r.status;}
function send(){let t=document.getElementById('txt').value;if(document.getElementById('enter').checked)t+='\n';post('/type',t);}
function key(k){post('/key',k);}
let SENS=parseFloat(localStorage.getItem('sens')||'1.6');
const sens=document.getElementById('sens'),sensv=document.getElementById('sensv');
sens.value=SENS;sensv.textContent=SENS.toFixed(1);
sens.addEventListener('input',()=>{SENS=parseFloat(sens.value);sensv.textContent=SENS.toFixed(1);localStorage.setItem('sens',sens.value);});
function clamp(v){v=v|0;return v<-127?-127:v>127?127:v;}
function mpost(a){fetch('/mouse',{method:'POST',body:new Uint8Array(a)});}
function mclick(b){mpost([2,0x63,b]);}
function mscroll(a){mpost([2,0x73,(clamp(a)+128)&255]);}
const pad=document.getElementById('pad');let pd=false,lx=0,ly=0,ax=0,ay=0,mv=false;
pad.addEventListener('pointerdown',e=>{pd=true;lx=e.clientX;ly=e.clientY;mv=false;ax=ay=0;pad.setPointerCapture(e.pointerId);e.preventDefault();});
pad.addEventListener('pointermove',e=>{if(!pd)return;ax+=e.clientX-lx;ay+=e.clientY-ly;lx=e.clientX;ly=e.clientY;if(Math.abs(ax)>3||Math.abs(ay)>3)mv=true;});
pad.addEventListener('pointerup',e=>{if(!pd)return;pd=false;if(!mv)mclick(1);});
setInterval(()=>{if(!ax&&!ay)return;const dx=clamp(Math.round(ax*SENS)),dy=clamp(Math.round(ay*SENS));ax=0;ay=0;if(dx||dy)mpost([2,0x6d,(dx+128)&255,(dy+128)&255]);},40);
</script></body></html>
)HTML";

void tapHidKey(uint8_t key, bool shift = false) {
  if (shift) Keyboard.press(KEY_LEFT_SHIFT);
  Keyboard.press(key);
  delay(HID_KEY_DOWN_MS);
  Keyboard.releaseAll();
  delay(HID_KEY_GAP_MS);
}

void pressWinSpace() {
  Keyboard.press(KEY_LEFT_GUI);
  Keyboard.press(' ');
  delay(80);
  Keyboard.releaseAll();
  delay(HID_KEY_GAP_MS);
}

void doKey(const String &k) {
  if (k == "ENTER")      tapHidKey(KEY_RETURN);
  else if (k == "TAB")   tapHidKey(KEY_TAB);
  else if (k == "ESC")   tapHidKey(KEY_ESC);
  else if (k == "WIN")   pressWinSpace();
  else if (k == "CTRL_ALT_DEL") {
    Keyboard.press(KEY_LEFT_CTRL); Keyboard.press(KEY_LEFT_ALT); Keyboard.press(KEY_DELETE);
    delay(80); Keyboard.releaseAll(); delay(HID_KEY_GAP_MS);
  }
}

// Map Unicode Russian letters to the physical keys of the Windows ЙЦУКЕН layout.
// The target computer must have RUS selected; the phone may send normal UTF-8 text.
bool russianKey(uint16_t cp, char &key, bool &shift) {
  shift = false;
  if (cp == 0x0401) { cp = 0x0451; shift = true; }       // Ё
  else if (cp >= 0x0410 && cp <= 0x042F) { cp += 0x20; shift = true; }

  switch (cp) {
    case 0x0430: key = 'f'; return true;  // а
    case 0x0431: key = ','; return true;  // б
    case 0x0432: key = 'd'; return true;  // в
    case 0x0433: key = 'u'; return true;  // г
    case 0x0434: key = 'l'; return true;  // д
    case 0x0435: key = 't'; return true;  // е
    case 0x0451: key = '`'; return true;  // ё
    case 0x0436: key = ';'; return true;  // ж
    case 0x0437: key = 'p'; return true;  // з
    case 0x0438: key = 'b'; return true;  // и
    case 0x0439: key = 'q'; return true;  // й
    case 0x043A: key = 'r'; return true;  // к
    case 0x043B: key = 'k'; return true;  // л
    case 0x043C: key = 'v'; return true;  // м
    case 0x043D: key = 'y'; return true;  // н
    case 0x043E: key = 'j'; return true;  // о
    case 0x043F: key = 'g'; return true;  // п
    case 0x0440: key = 'h'; return true;  // р
    case 0x0441: key = 'c'; return true;  // с
    case 0x0442: key = 'n'; return true;  // т
    case 0x0443: key = 'e'; return true;  // у
    case 0x0444: key = 'a'; return true;  // ф
    case 0x0445: key = '['; return true;  // х
    case 0x0446: key = 'w'; return true;  // ц
    case 0x0447: key = 'x'; return true;  // ч
    case 0x0448: key = 'i'; return true;  // ш
    case 0x0449: key = 'o'; return true;  // щ
    case 0x044A: key = ']'; return true;  // ъ
    case 0x044B: key = 's'; return true;  // ы
    case 0x044C: key = 'm'; return true;  // ь
    case 0x044D: key = '\''; return true; // э
    case 0x044E: key = '.'; return true;  // ю
    case 0x044F: key = 'z'; return true;  // я
    default: return false;
  }
}

void writePhysicalKey(char key, bool shift) {
  tapHidKey((uint8_t)key, shift);
}

void writeRussianPunctuation(uint8_t c) {
  switch (c) {
    case '.': writePhysicalKey('/', false); break;
    case ',': writePhysicalKey('/', true);  break;
    case ':': writePhysicalKey('6', true);  break;
    case ';': writePhysicalKey('4', true);  break;
    case '?': writePhysicalKey('7', true);  break;
    case '"': writePhysicalKey('2', true);  break;
    default:  tapHidKey(c);                  break;
  }
}

void typeUtf8(const uint8_t *data, size_t length) {
  for (size_t i = 0; i < length; ++i) {
    uint8_t b = data[i];

    if (pendingUtf8Lead) {
      if ((b & 0xC0) == 0x80) {
        uint16_t cp = ((uint16_t)(pendingUtf8Lead & 0x1F) << 6) | (b & 0x3F);
        char key = 0;
        bool shift = false;
        if (russianKey(cp, key, shift)) {
          russianTextContext = true;
          writePhysicalKey(key, shift);
        }
      }
      pendingUtf8Lead = 0;
      continue;
    }

    if (b == 0xD0 || b == 0xD1) {
      pendingUtf8Lead = b;
      continue;
    }

    if (b < 0x80) {
      if ((b >= 'A' && b <= 'Z') || (b >= 'a' && b <= 'z')) russianTextContext = false;
      if (russianTextContext && (b == '.' || b == ',' || b == ':' || b == ';' || b == '?' || b == '"')) {
        writeRussianPunctuation(b);
      } else {
        tapHidKey(b);
      }
    }
  }
}

// Mouse command bytes (all non-zero): 0x02 'm' dx+128 dy+128 | 'c' btn | 's' amt+128
void doMouse(const uint8_t *d, size_t n) {
  if (n < 2) return;
  if (d[1] == 'm' && n >= 4)      Mouse.move((int)d[2] - 128, (int)d[3] - 128, 0);
  else if (d[1] == 'c' && n >= 3) Mouse.click(d[2]);
  else if (d[1] == 's' && n >= 3) Mouse.move(0, 0, (int)d[2] - 128);
}

// shared screen content -> board renders it (boardRenderScreen lives in boards/<board>.h)
void drawScreen() {
  if (g_showPin) {
    char pin[8]; snprintf(pin, sizeof(pin), "%06u", (unsigned)g_pairPin);
    boardRenderScreen("PAIRING", DC_MAGENTA, "PIN:", String(pin), "");
  } else if (radioMode == 0) {
    boardRenderScreen("WiFi MODE", DC_CYAN, "Keyboard", apPass, "192.168.4.1");
  } else {
    boardRenderScreen("BLE MODE", DC_GREEN, "Keyboard",
                      bleConnected ? "connected" : "advertising", "btn=WiFi");
  }
}

void genCreds() {
  // UTF-8 name used by both BLE advertising and the Wi-Fi access point.
  apSsid = "\xD0\x9A\xD0\xBB\xD0\xB0\xD0\xB2\xD0\xB8\xD0\xB0\xD1\x82\xD1\x83\xD1\x80\xD0\xB0";
  apPass = "12345678";
  Serial.printf("[%s] WiFi: %s / %s  ->  http://192.168.4.1\n", BOARD_NAME, apSsid.c_str(), apPass.c_str());
}

void startAP() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP(apSsid.c_str(), apPass.c_str());
  wifiApUp = true;
}
void stopAP() {
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_OFF);
  wifiApUp = false;
  wifiClients = 0;
}
void onWiFiEvent(WiFiEvent_t e) {
  if (e == ARDUINO_EVENT_WIFI_AP_STACONNECTED) wifiClients++;
  else if (e == ARDUINO_EVENT_WIFI_AP_STADISCONNECTED && wifiClients > 0) wifiClients--;
}
void startAdv() { if (!bleAdvOn) { advertising->start(); bleAdvOn = true; } }
void stopAdv()  { if (bleAdvOn)  { advertising->stop();  bleAdvOn = false; } }

class SrvCb : public BLEServerCallbacks {
  void onConnect(BLEServer *s) override {
    (void)s;
    bleConnected = true;
    bleAdvOn = false;  // BLE advertising stops after a connection is made.
    needRedraw = true;
  }

  void onDisconnect(BLEServer *s) override {
    (void)s;
    bleConnected = false;
    bleAdvOn = false;

    // Make the keyboard discoverable again without rebooting the dongle.
    if (radioMode == 1 && advertising != nullptr) {
      advertising->start();
      bleAdvOn = true;
    }

    needRedraw = true;
  }
};
class RxCb : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *c) override {
    if (g_showPin) { g_showPin = false; needRedraw = true; }
    std::string raw = c->getValue();
    if (raw.empty()) return;
    if ((uint8_t)raw[0] == 0x01) {
      doKey(String(raw.c_str() + 1));
    } else if ((uint8_t)raw[0] == 0x02) {
      doMouse((const uint8_t *)raw.data(), raw.size());
    } else {
      typeUtf8((const uint8_t *)raw.data(), raw.size());
    }
  }
};
#if REQUIRE_BLE_PAIRING
class SecCb : public BLESecurityCallbacks {
  uint32_t onPassKeyRequest() override { return 0; }
  void onPassKeyNotify(uint32_t pk) override { g_pairPin = pk; g_showPin = true; needRedraw = true; }
  bool onConfirmPIN(uint32_t pk) override { return true; }
  bool onSecurityRequest() override { return true; }
};
#endif

void applyRadio() {
  if (radioMode == 0) { stopAdv(); if (!wifiApUp) startAP(); }
  else                { stopAP();  startAdv(); }
  needRedraw = true;
}
void pollButton() {
  static bool last = HIGH;
  static uint32_t t = 0;
  bool b = digitalRead(BTN_PIN);
  if (b != last && millis() - t > 60) {
    t = millis(); last = b;
    if (b == LOW) { radioMode = !radioMode; applyRadio(); }
  }
}

void setup() {
  Serial.begin(115200);
  Keyboard.begin();
  Mouse.begin();
  USB.begin();

  boardInitDisplay();
  boardShowSplash();
  delay(4000);
  genCreds();

  pinMode(BTN_PIN, INPUT_PULLUP);
  WiFi.onEvent(onWiFiEvent);
  startAP();                        // WiFi up before BLE init (coexistence order)

  BLEDevice::init(apSsid.c_str());
  BLEServer *srv = BLEDevice::createServer();
  srv->setCallbacks(new SrvCb());
  BLEService *svc = srv->createService(SERVICE_UUID);
  BLECharacteristic *rx = svc->createCharacteristic(
      CHAR_RX_UUID, BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR);
#if REQUIRE_BLE_PAIRING
  rx->setAccessPermissions(ESP_GATT_PERM_WRITE_ENC_MITM);
#endif
  rx->setCallbacks(new RxCb());
  BLECharacteristic *tx = svc->createCharacteristic(CHAR_TX_UUID, BLECharacteristic::PROPERTY_NOTIFY);
  tx->addDescriptor(new BLE2902());
  svc->start();

  advertising = BLEDevice::getAdvertising();
  advertising->addServiceUUID(SERVICE_UUID);
  advertising->setScanResponse(true);

#if REQUIRE_BLE_PAIRING
  BLEDevice::setSecurityCallbacks(new SecCb());
  BLESecurity *sec = new BLESecurity();
  sec->setAuthenticationMode(ESP_LE_AUTH_REQ_SC_MITM_BOND);
  sec->setCapability(ESP_IO_CAP_OUT);
  sec->setInitEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);
  sec->setRespEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);
#endif

  server.on("/", []() { server.send_P(200, "text/html", INDEX_HTML); });
  server.on("/type", HTTP_POST, []() {
    String body = server.arg("plain");
    typeUtf8((const uint8_t *)body.c_str(), body.length());
    server.send(200, "text/plain", "ok");
  });
  server.on("/key",  HTTP_POST, []() { doKey(server.arg("plain")); server.send(200, "text/plain", "ok"); });
  server.on("/mouse", HTTP_POST, []() { String b = server.arg("plain"); doMouse((const uint8_t *)b.c_str(), b.length()); server.send(200, "text/plain", "ok"); });
  server.begin();

  radioMode = 1;
  applyRadio();
  drawScreen();
}

void loop() {
  server.handleClient();
  pollButton();
  if (needRedraw) { needRedraw = false; drawScreen(); }
  static uint32_t t = 0;
  if (millis() - t > 5000) {
    t = millis();
    Serial.printf("[%s] mode=%s WiFi: %s / %s\n", BOARD_NAME,
                  radioMode == 0 ? "WiFi" : "BLE", apSsid.c_str(), apPass.c_str());
  }
  delay(10);
}
