// M5Stack AtomS3U — board-specific config (no screen, just a button + LED).
// Isolated here so changes to this board can't affect the others.
#pragma once

#define BOARD_NAME  "M5Stack AtomS3U"
#define HAS_DISPLAY 0
#define BTN_PIN     41         // AtomS3U button

inline void boardInitDisplay() {}   // no display
inline void boardRenderScreen(const String &, uint16_t, const String &, const String &, const String &) {}
