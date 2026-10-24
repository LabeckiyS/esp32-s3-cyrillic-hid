// LilyGo T-Dongle S3 — board-specific config (pins + ST7735 display).
// Isolated here so changes to this board can't affect the others.
#pragma once
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include "cyber_star_v8_splash.h"

#define BOARD_NAME  "LilyGo T-Dongle S3"
#define HAS_DISPLAY 1
#define BTN_PIN     0          // side button

// ST7735 (0.96" 80x160) pins
#define TFT_CS   4
#define TFT_DC   2
#define TFT_RST  1
#define TFT_MOSI 3
#define TFT_SCLK 5
#define TFT_BL   38
#define TXT_SIZE 1

static Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCLK, TFT_RST);

inline void boardInitDisplay() {
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, LOW);   // backlight is active-low on this board (per LilyGo's own example)
  // PLUGIN variant matches LilyGo's official TFT_eSPI setup for this panel:
  // BGR color order + 26px offset + inversion (INITR_MINI160x80 alone is RGB/24px/no-invert, which is wrong here)
  tft.initR(INITR_MINI160x80_PLUGIN);
  // Portrait orientation: the top of the artwork points away from the USB plug.
  tft.setRotation(0);
  tft.fillScreen(ST77XX_BLACK);
}

inline void boardShowSplash() {
  tft.drawRGBBitmap(0, 0, cyberStarSplash,
                    CYBER_STAR_SPLASH_WIDTH, CYBER_STAR_SPLASH_HEIGHT);
}

inline void boardRenderScreen(const String &title, uint16_t color,
                              const String &l2, const String &l3, const String &l4) {
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextWrap(false);
  tft.setTextSize(TXT_SIZE);
  const int lh = 24;
  int y = 22;
  tft.setTextColor(color);         tft.setCursor(3, y); tft.print(title); y += lh + 4;
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(3, y); tft.print(l2); y += lh;
  tft.setCursor(3, y); tft.print(l3); y += lh;
  tft.setCursor(3, y); tft.print(l4);
}
