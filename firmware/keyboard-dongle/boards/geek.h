// Waveshare ESP32-S3-Geek — board-specific config (pins + ST7789 display).
// Isolated here so changes to this board can't affect the others.
#pragma once
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>

#define BOARD_NAME  "Waveshare ESP32-S3-Geek"
#define HAS_DISPLAY 1
#define BTN_PIN     0          // BOOT button

// ST7789 pins (per esp-cpp/espp ws-s3-geek driver)
#define TFT_CS   10
#define TFT_DC   8
#define TFT_RST  9
#define TFT_MOSI 11
#define TFT_SCLK 12
#define TFT_BL   7
#define TXT_SIZE 2

// software-SPI constructor (exact pins) — avoids Adafruit re-begin()'ing HW SPI on the wrong pins
static Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCLK, TFT_RST);

inline void boardInitDisplay() {
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);
  tft.init(135, 240);          // 1.14" 240x135 panel
  tft.setRotation(3);
  tft.fillScreen(ST77XX_BLACK);
}

inline void boardRenderScreen(const String &title, uint16_t color,
                              const String &l2, const String &l3, const String &l4) {
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextSize(TXT_SIZE);
  const int lh = 8 * TXT_SIZE + 4;
  int y = 4;
  tft.setTextColor(color);         tft.setCursor(3, y); tft.print(title); y += lh + 4;
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(3, y); tft.print(l2); y += lh;
  tft.setCursor(3, y); tft.print(l3); y += lh;
  tft.setCursor(3, y); tft.print(l4);
}
