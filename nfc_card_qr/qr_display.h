#ifndef QR_DISPLAY_H
#define QR_DISPLAY_H

#include <Arduino.h>
#include <Adafruit_SSD1306.h>

// Renders `text` as a QR code, scaled up and centered on a 128x64 SSD1306.
// Text longer than ~106 bytes is truncated (see qr_display.cpp) to keep
// the QR version - and its decode buffer - small enough for Uno SRAM.
void showQRCode(Adafruit_SSD1306 &display, const char *text);

#endif
