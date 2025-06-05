/*
 * blink.ino
 * Copyright (C) Seeed K.K.
 * MIT License
 */

#include <Adafruit_TinyUSB.h>

void setup() {
}

void loop() {
  digitalWrite(LED_BUILTIN, HIGH);
  delay(200);
  digitalWrite(LED_BUILTIN, LOW);
  delay(800);
}
