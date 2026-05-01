/*
 * vsys.ino
 * Copyright (C) Seeed K.K.
 * MIT License
 */

#include <Adafruit_TinyUSB.h>

#define INTERVAL (500)

void setup() {
  Serial.begin(115200);
  {
    const auto start = millis();
    while (!Serial && millis() - start < 5000) {
      delay(2);
    }
  }
  Serial.println();
  Serial.println();

  analogReadResolution(14);
  analogSampleTime(40);
  analogOversampling(256);
}

void loop() {
#ifdef PIN_VSYS_DIV
  const auto raw = analogRead(PIN_VSYS_DIV);

  // Convert raw value to voltage
  //
  // Voltage = Value / Resolution * Reference voltage / Gain
  //   Resolution       : 16383 ... 14-bit resolution = 2^14-1
  //   Reference voltage: 0.6   ... Internal reference = 0.6V
  //   Gain             : 1/6   ... 1/6
  const auto voltage = (float)raw / 16383 * 0.6f / (1.0f / 6);

  const auto vsys = voltage / 130.0f * (750.0f + 130.0f);

  Serial.println(vsys, 3);
#else
  Serial.println("This board version does not support VSYS measurement.");
#endif  // PIN_VSYS_DIV

  delay(INTERVAL);
}
