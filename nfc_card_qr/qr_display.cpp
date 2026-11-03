#include "qr_display.h"
#include <qrcode.h>

// Max QR version we'll ever request, chosen to bound RAM use: version 5's
// module buffer is ~172 bytes, on top of the SSD1306's 1024-byte
// framebuffer that's most of the Uno's 2KB SRAM already.
#define MAX_QR_VERSION 5
#define MAX_TEXT_LEN 106 // byte-mode ECC_LOW capacity at version 5

static uint8_t versionForLength(size_t len) {
  if (len <= 17) return 1;
  if (len <= 32) return 2;
  if (len <= 53) return 3;
  if (len <= 78) return 4;
  return MAX_QR_VERSION;
}

void showQRCode(Adafruit_SSD1306 &display, const char *text) {
  char truncated[MAX_TEXT_LEN + 1];
  snprintf(truncated, sizeof(truncated), "%s", text);

  uint8_t version = versionForLength(strlen(truncated));
  uint8_t qrBuf[qrcode_getBufferSize(MAX_QR_VERSION)];
  QRCode qrcode;
  qrcode_initText(&qrcode, qrBuf, version, ECC_LOW, truncated);

  uint8_t modules = qrcode.size;
  uint8_t scale = 1;
  while (modules * (scale + 1) <= 64) scale++;

  uint8_t qrPixels = modules * scale;
  int16_t xOff = (128 - qrPixels) / 2;
  int16_t yOff = (64 - qrPixels) / 2;

  display.clearDisplay();
  for (uint8_t y = 0; y < modules; y++) {
    for (uint8_t x = 0; x < modules; x++) {
      if (qrcode_getModule(&qrcode, x, y)) {
        display.fillRect(xOff + x * scale, yOff + y * scale, scale, scale, SSD1306_WHITE);
      }
    }
  }
  display.display();
}
