/*
 * grove-rotary-angle-sensor.ino
 * Copyright (C) Seeed K.K.
 * MIT License
 */

#include <Adafruit_TinyUSB.h>
#include <WioCellular.h>

#define ROTARY_ANGLE_PIN (A4)  // Grove - Analog (P1)
#define INTERVAL (500)
#define BAR_LENGTH (40)

void setup(void) {
  Serial.begin(115200);
  {
    const auto start = millis();
    while (!Serial && millis() - start < 5000) {
      delay(2);
    }
  }
  Serial.println();
  Serial.println();

  WioCellular.begin();
  digitalWrite(PIN_VGROVE_ENABLE, VGROVE_ENABLE_ON);
  delay(2 + 2);

  analogReadResolution(14);
}

void loop(void) {
  const auto rotaryAngleRaw = analogRead(ROTARY_ANGLE_PIN);

  // Convert raw value to voltage
  //
  // Voltage = Value / Resolution * Reference voltage / Gain
  //   Resolution       : 16383 ... 14-bit resolution = 2^14-1
  //   Reference voltage: 0.6   ... Internal reference = 0.6V
  //   Gain             : 1/6   ... 1/6
  const auto rotaryAngleVoltage = (float)rotaryAngleRaw / 16383 * 0.6f / (1.0f / 6);

  // Normalize 0~3.3V to 0~1
  const auto rotaryAngle = rotaryAngleVoltage / 3.3f;

  int i;
  for (i = 0; i < BAR_LENGTH * rotaryAngle; i++) Serial.print("*");
  for (; i < BAR_LENGTH; i++) Serial.print(".");
  Serial.print(" ");
  Serial.print(rotaryAngle);
  Serial.print("(");
  Serial.print(rotaryAngleRaw);
  Serial.println(")");

  delay(INTERVAL);
}
