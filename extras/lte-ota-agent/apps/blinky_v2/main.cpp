#include <Arduino.h>
#include <Adafruit_TinyUSB.h>

namespace {
constexpr uint32_t kVersion = 2;
}

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  Serial.begin(115200);
  const uint32_t wait_started = millis();
  while (!Serial && millis() - wait_started < 5000) {
    delay(10);
  }
  Serial.print("Wio BG770A OTA target version ");
  Serial.println(kVersion);
}

void loop() {
  digitalWrite(LED_BUILTIN, LED_STATE_ON);
  delay(800);
  digitalWrite(LED_BUILTIN, !LED_STATE_ON);
  delay(200);
}
