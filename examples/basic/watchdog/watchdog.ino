/*
 * watchdog.ino
 * Copyright (C) Seeed K.K.
 * MIT License
 */

////////////////////////////////////////////////////////////////////////////////
// Libraries:
//   https://github.com/matsujirushi/Adafruit_SleepyDog master

#include <Adafruit_TinyUSB.h>
#include <Adafruit_SleepyDog.h>
#include <string>

static uint32_t start;

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

  const auto resetReason = readResetReason();
  std::string resetReasonStr;
  if (resetReason & POWER_RESETREAS_RESETPIN_Msk) {
    if (!resetReasonStr.empty()) resetReasonStr += ", ";
    resetReasonStr += "Pin";
  }
  if (resetReason & POWER_RESETREAS_DOG_Msk) {
    if (!resetReasonStr.empty()) resetReasonStr += ", ";
    resetReasonStr += "Watchdog";
  }
  if (resetReason & POWER_RESETREAS_SREQ_Msk) {
    if (!resetReasonStr.empty()) resetReasonStr += ", ";
    resetReasonStr += "Soft";
  }
  if (resetReason & POWER_RESETREAS_LOCKUP_Msk) {
    if (!resetReasonStr.empty()) resetReasonStr += ", ";
    resetReasonStr += "CpuLockUp";
  }

  if (resetReasonStr.empty()) {
    Serial.printf("Reset reason: 0x%x\n", resetReason);
  } else {
    Serial.printf("Reset reason: 0x%x(%s)\n", resetReason, resetReasonStr.c_str());
  }

  digitalWrite(LED_BUILTIN, HIGH);
  Serial.println("Enable watchdog");
  Watchdog.enable(10000);
  start = millis();
}

void loop(void) {
  Serial.println(millis() - start);

  if (digitalRead(PIN_BUTTON1) == LOW) {
    Serial.println("Reset watchdog");
    Watchdog.reset();
  }

  delay(1000);
}
