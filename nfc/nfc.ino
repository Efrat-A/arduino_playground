// The program is plain C: see nfc_app.c (serial commands) and rc522.c (reader driver).
// No extra libraries needed. Wiring and commands are in README.md.

#include "nfc_app.h"

void setup() {
  nfc_app_init();
}

void loop() {
  nfc_app_poll();
}
