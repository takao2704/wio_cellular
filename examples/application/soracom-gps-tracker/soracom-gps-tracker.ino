/*
 * soracom-gps-tracker.ino
 * Copyright (C) Seeed K.K.
 * MIT License
 */

////////////////////////////////////////////////////////////////////////////////
// Libraries:
//   http://librarymanager#ArduinoJson 7.0.4
//   http://librarymanager#Adafruit%20SPIFlash 4.3.4
//   http://librarymanager#SdFat%20-%20Adafruit%20Fork 2.2.3

#include <Adafruit_TinyUSB.h>
#include <cassert>
#include <csignal>
#include <malloc.h>
#include "Storage.hpp"
#include "CellularTask.hpp"
#include "MeasureTask.hpp"

#define TASK_NAME "[main]"

static constexpr int INTERVAL = 1000 * 60 * 60;  // [ms]

static void abortHandler(int sig) {
  while (true) {
    ledOn(LED_BUILTIN);
    delay(100);
    ledOff(LED_BUILTIN);
    delay(100);
  }
}

static TaskHandle_t CellularTaskHandle;  // FreeRTOS
static TaskHandle_t MeasureTaskHandle;   // FreeRTOS

void setup(void) {
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

  Serial.println(TASK_NAME "Startup");
  digitalWrite(LED_BUILTIN, HIGH);

  // Check and clear storage
  if (!Storage::begin()) abort();
  uint32_t marker;
  if (!Storage::readMarker(&marker)) abort();
  if (marker != 0x12345678) {
    if (!Storage::clear()) abort();
    if (!Storage::writeMarker(0x12345678)) abort();
  }

  // Begin tasks
  CellularTaskBegin();
  MeasureTaskBegin();

  // Start tasks
  if (xTaskCreate(CellularTaskFunction, "Cellular", 600, nullptr, 1, &CellularTaskHandle) != pdPASS) abort();  // FreeRTOS
  if (xTaskCreate(MeasureTaskFunction, "Measure", 600, nullptr, 2, &MeasureTaskHandle) != pdPASS) abort();     // FreeRTOS

  digitalWrite(LED_BUILTIN, LOW);
}

void loop(void) {
  diagnostics();
  delay(INTERVAL);
}

void diagnostics(void) {
  Serial.println(TASK_NAME "Diagnostics start");

  // Stack
  const auto taskNumber = uxTaskGetNumberOfTasks();
  TaskStatus_t* taskStatuses = reinterpret_cast<TaskStatus_t*>(pvPortMalloc(sizeof(TaskStatus_t) * taskNumber));
  assert(uxTaskGetSystemState(taskStatuses, taskNumber, nullptr) == taskNumber);
  for (size_t i = 0; i < taskNumber; ++i) {
    Serial.printf(TASK_NAME "stack_hwm %-7s %u\n", taskStatuses[i].pcTaskName, static_cast<unsigned>(taskStatuses[i].usStackHighWaterMark));
  }
  vPortFree(taskStatuses);

  // Heap
  struct mallinfo info = mallinfo();
  Serial.printf(TASK_NAME "heap_used %u\n", info.uordblks);

  Serial.println(TASK_NAME "Diagnostics end");
}
