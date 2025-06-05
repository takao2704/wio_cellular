/*
 * MeasureTask.cpp
 * Copyright (C) Seeed K.K.
 * MIT License
 */

#include <Arduino.h>
#include "MeasureTask.hpp"
#include <ArduinoJson.h>
#include "TaskSafeStorage.hpp"

#define TASK_NAME "[meas]"

static constexpr int INTERVAL = 1000 * 60 * 5;  // [ms]
static constexpr int POLLING_GPS = 1000 * 60;   // [ms]

static String LatestGpsData;

static bool measure(JsonDocument& doc);
static void GpsBegin();
static void GpsEnd();
static const char* GpsRead();

void MeasureTaskBegin() {
}

void MeasureTaskFunction(void* param) {
  while (true) {
    // Power on the grove module
    Serial.println(TASK_NAME "Power on the grove module");
    digitalWrite(PIN_VGROVE_ENABLE, VGROVE_ENABLE_ON);
    delay(2 + 2);
    GpsBegin();

    // Polling GPS data
    Serial.println(TASK_NAME "Polling GPS data");
    LatestGpsData = "";
    const auto start = millis();
    while (millis() - start < POLLING_GPS) {
      const auto data = GpsRead();
      if (data != NULL && strncmp(data, "$GPGGA,", 7) == 0) {
        LatestGpsData = data;
      }
      delay(10);  // Spin
    }

    // Measure
    Serial.println(TASK_NAME "Measuring");
    JsonDocument JsonDoc;
    const bool measureResult = measure(JsonDoc);

    // Power off the grove module
    Serial.println(TASK_NAME "Power off the grove module");
    GpsEnd();
    digitalWrite(PIN_VGROVE_ENABLE, VGROVE_ENABLE_OFF);
    delay(2 + 2);

    if (measureResult) {
      // Print free size of storage
      Serial.printf(TASK_NAME "Free size of storage: %d\n", TaskSafeStorage::SendQueue::freeSize());

      // Append data to storage
      std::string str;
      serializeJson(JsonDoc, str);
      Serial.printf(TASK_NAME "Append %d bytes to storage %s\n", str.size(), str.c_str());
      if (!TaskSafeStorage::SendQueue::write(str.data(), str.size())) Serial.println(TASK_NAME "ERROR: Failed to write to storage");
    }

    delay(INTERVAL - POLLING_GPS);
  }
}

static bool measure(JsonDocument& doc) {
  const auto now = time(nullptr);
  if (now <= 0) return false;

  doc["time"] = now;
  doc["uptime"] = millis() / 1000;

  int index[5];
  index[0] = LatestGpsData.indexOf(',');
  index[1] = index[0] >= 0 ? LatestGpsData.indexOf(',', index[0] + 1) : -1;
  index[2] = index[1] >= 0 ? LatestGpsData.indexOf(',', index[1] + 1) : -1;
  index[3] = index[2] >= 0 ? LatestGpsData.indexOf(',', index[2] + 1) : -1;
  index[4] = index[3] >= 0 ? LatestGpsData.indexOf(',', index[3] + 1) : -1;

  if (index[4] >= 0) {
    String latDmm = LatestGpsData.substring(index[1] + 1, index[2]);
    String lonDmm = LatestGpsData.substring(index[3] + 1, index[4]);

    if (latDmm.length() >= 1 && lonDmm.length() >= 1) {
      auto DMMtoDD = [](const String& dmm) -> double {
        const double dmmDouble = atof(dmm.c_str());
        const int d = (int)dmmDouble / 100;
        return (double)d + (dmmDouble - d * 100) / 60.0;
      };

      doc["lat"] = DMMtoDD(latDmm);
      doc["lon"] = DMMtoDD(lonDmm);
    }
  }

  return true;
}

#define GPS_OVERFLOW_STRING "OVERFLOW"

static char GpsData[100];
static int GpsDataLength;

static void GpsBegin() {
  Serial1.begin(9600);
  GpsDataLength = 0;
}

static void GpsEnd() {
  Serial1.end();
}

static const char* GpsRead() {
  while (true) {
    const auto data = Serial1.read();
    if (data < 0) return NULL;
    if (data == '\r') continue;
    if (data == '\n') {
      GpsData[GpsDataLength] = '\0';
      GpsDataLength = 0;
      return GpsData;
    }

    if (GpsDataLength > (int)sizeof(GpsData) - 1) {  // Overflow
      GpsDataLength = 0;
      return GPS_OVERFLOW_STRING;
    }
    GpsData[GpsDataLength++] = data;
  }

  return NULL;
}
