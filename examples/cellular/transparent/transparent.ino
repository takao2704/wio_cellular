/*
 * transparent.ino
 * Copyright (C) Seeed K.K.
 * MIT License
 */

#include <Adafruit_TinyUSB.h>
#include <csignal>
#include <WioCellular.h>

static constexpr int POWER_ON_TIMEOUT = 1000 * 20;  // [ms]

static void abortHandler(int sig) {
  Serial.printf("ABORT: Signal %d received\n", sig);
  yield();

  vTaskSuspendAll();  // FreeRTOS
  while (true) {
    ledOn(LED_BUILTIN);
    nrfx_coredep_delay_us(100000);  // Spin
    ledOff(LED_BUILTIN);
    nrfx_coredep_delay_us(100000);  // Spin
  }
}

void setup() {
  signal(SIGABRT, abortHandler);
  Serial.begin(115200);
  {
    const auto start = millis();
    while (!Serial && millis() - start < 5000) {
      delay(2);
    }
  }
  Serial.println();
  Serial.println();

  Serial.println("Initialize");
  WioCellular.begin();

  Serial.println("Turn on cellular");
  if (WioCellular.powerOn(POWER_ON_TIMEOUT) != WioCellularResult::Ok) {
    Serial.println("ERROR");
    abort();
  }

  Serial.println("Ready");
}

void loop() {
  int c;

  while (true) {
    while ((c = Serial.read()) >= 0) {
      WioCellular.getInterface().write(c);
    }

    while ((c = WioCellular.getInterface().read()) >= 0) {
      Serial.write(c);
    }

    delay(2);
  }
}
