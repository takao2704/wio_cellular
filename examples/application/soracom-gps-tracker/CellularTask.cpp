/*
 * CellularTask.cpp
 * Copyright (C) Seeed K.K.
 * MIT License
 */

#include <Arduino.h>
#include "CellularTask.hpp"
#include <WioCellular.h>
#include "Storage.hpp"

#define TASK_NAME "[cell]"
#define USE_PSM

#define SEARCH_ACCESS_TECHNOLOGY (WioCellularNetwork::SearchAccessTechnology::LTEM)
#define LTEM_BAND (WioCellularNetwork::NTTDOCOMO_LTEM_BAND)
static const char APN[] = "soracom.io";

static const char HOST[] = "uni.soracom.io";
static constexpr int PORT = 23080;

static constexpr int INTERVAL = 1000 * 60 * 15;           // [ms]
static constexpr int POWER_ON_TIMEOUT = 1000 * 20;        // [ms]
static constexpr int NETWORK_TIMEOUT = 1000 * 60 * 2;     // [ms]
static constexpr int RECEIVE_TIMEOUT = 1000 * 10;         // [ms]
static constexpr int PSM_PERIOD = 60 * 17;                // [s]
static constexpr int PSM_ACTIVE = 2;                      // [s]
static constexpr int PSM_POWER_DOWN_TIMEOUT = 1000 * 60;  // [ms]
static constexpr int POWER_OFF_DELAY_TIME = 1000 * 3;     // [ms]

static bool send(const void* data, size_t dataSize);

void CellularTaskBegin(void) {
  // Network configuration
  WioNetwork.config.searchAccessTechnology = SEARCH_ACCESS_TECHNOLOGY;
  WioNetwork.config.ltemBand = LTEM_BAND;
  WioNetwork.config.apn = APN;

  // Start WioCellular
  WioCellular.begin();
}

void CellularTaskFunction(void* param) {
  while (true) {
    // Peek data from storage
    uint16_t readDataSize;
    if (!Storage::peek(nullptr, 0, &readDataSize)) abort();
    Serial.printf(TASK_NAME "Peek %d bytes from storage\n", readDataSize);
    if (readDataSize >= 1) {
      // Power on the cellular module
      Serial.println(TASK_NAME "Power on the cellular module");
      if (WioCellular.powerOn(POWER_ON_TIMEOUT) != WioCellularResult::Ok) abort();
      WioNetwork.begin();

#ifdef USE_PSM
      // Reset PSM
      if (WioCellular.setPsmEnteringIndicationUrc(true) != WioCellularResult::Ok) abort();
      if (WioCellular.setPsm(0, PSM_PERIOD, PSM_ACTIVE) != WioCellularResult::Ok) abort();
#endif  // USE_PSM

      // Send
      if (WioNetwork.waitUntilCommunicationAvailable(NETWORK_TIMEOUT)) {
        while (true) {
          // Peek data from storage
          uint16_t readDataSize;
          if (!Storage::peek(nullptr, 0, &readDataSize)) abort();
          Serial.printf(TASK_NAME "Peek %d bytes from storage\n", readDataSize);
          if (readDataSize == 0) break;
          std::unique_ptr<uint8_t[]> data = std::make_unique<uint8_t[]>(readDataSize + 1);
          if (!Storage::peek(data.get(), readDataSize, &readDataSize)) abort();
          data[readDataSize + 1] = '\0';
          Serial.printf(TASK_NAME "Peeked %s\n", data.get());

          // Send data
          if (!send(data.get(), readDataSize)) break;

          // Processed data from storage
          if (!Storage::read(nullptr, 0, &readDataSize)) abort();
          Serial.printf(TASK_NAME "Processed %d bytes from storage\n", readDataSize);
        }
      }

      // Power off the cellular module
      Serial.println(TASK_NAME "Power off the cellular module");
#ifdef USE_PSM
      bool powerDown = false;
      if (WioNetwork.canCommunicate()) {
        // Set PSM
        if (WioCellular.setPsm(1, PSM_PERIOD, PSM_ACTIVE) != WioCellularResult::Ok) abort();
        const auto start = millis();
        while (millis() - start < PSM_POWER_DOWN_TIMEOUT) {
          WioCellular.doWork(10);  // Spin
          if (!WioCellular.getInterface().isActive()) {
            powerDown = true;
            break;
          }
        }
      }
      if (!powerDown) {
        WioNetwork.end();
        if (WioCellular.powerOff() != WioCellularResult::Ok) abort();
      }
#else
      WioCellular.doWorkUntil(POWER_OFF_DELAY_TIME);
      WioNetwork.end();
      if (WioCellular.powerOff() != WioCellularResult::Ok) abort();
#endif  // USE_PSM
    }

    WioCellular.doWorkUntil(INTERVAL);
  }
}

template<typename T>
static void printData(T& stream, const void* data, size_t size) {
  auto p = static_cast<const char*>(data);

  for (; size > 0; --size, ++p)
    // stream.write(0x20 <= *p && *p <= 0x7f ? *p : '.');
    stream.printf("%02x ", *p);
}

static bool send(const void* data, size_t dataSize) {
  Serial.printf(TASK_NAME "Connecting %s:%d\n", HOST, PORT);

  {
    WioCellularTcpClient2<WioCellularModule> client{ WioCellular };
    if (!client.open(WioNetwork.config.pdpContextId, HOST, PORT)) {
      Serial.printf(TASK_NAME "ERROR: Failed to open %s\n", WioCellularResultToString(client.getLastResult()));
      return false;
    }

    if (!client.waitforConnect()) {
      Serial.printf(TASK_NAME "ERROR: Failed to connect %s\n", WioCellularResultToString(client.getLastResult()));
      return false;
    }

    Serial.println(TASK_NAME "Sending");
    if (!client.send(data, dataSize)) {
      Serial.printf(TASK_NAME "ERROR: Failed to send socket %s\n", WioCellularResultToString(client.getLastResult()));
      return false;
    }

    Serial.println(TASK_NAME "Receiving");
    static uint8_t recvData[WioCellular.RECEIVE_SOCKET_SIZE_MAX];
    size_t recvSize;
    if (!client.receive(recvData, sizeof(recvData), &recvSize, RECEIVE_TIMEOUT)) {
      Serial.printf(TASK_NAME "ERROR: Failed to receive socket %s\n", WioCellularResultToString(client.getLastResult()));
      return false;
    }

    Serial.print(TASK_NAME "Received ");
    printData(Serial, recvData, recvSize);
    Serial.println();
  }

  return true;
}
