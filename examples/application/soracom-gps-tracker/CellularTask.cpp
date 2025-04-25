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
// #define USE_PSM

#define SEARCH_ACCESS_TECHNOLOGY (WioCellularNetwork::SearchAccessTechnology::LTEM)  // https://seeedjp.github.io/Wiki/Wio_BG770A/kb/kb4.html
#define LTEM_BAND (WioCellularNetwork::NTTDOCOMO_LTEM_BAND)                          // https://seeedjp.github.io/Wiki/Wio_BG770A/kb/kb4.html
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
static constexpr int BATCH_SIZE = 6;

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
    // Check if data is in storage
    Serial.println(TASK_NAME "Check if data is in storage");
    uint16_t readDataSize;
    if (!Storage::peek(nullptr, 0, &readDataSize)) abort();

    if (time(nullptr) == 0 || readDataSize >= 1) {
      // Power on the cellular module
      Serial.println(TASK_NAME "Power on the cellular module");
      if (WioCellular.powerOn(POWER_ON_TIMEOUT) != WioCellularResult::Ok) abort();
      WioNetwork.begin();

#ifdef USE_PSM
      // Reset PSM
      if (WioCellular.setPsmEnteringIndicationUrc(true) != WioCellularResult::Ok) abort();
      if (WioCellular.setPsm(0, PSM_PERIOD, PSM_ACTIVE) != WioCellularResult::Ok) abort();
#endif  // USE_PSM

      // Update clock
      Serial.println(TASK_NAME "Update clock");
      time_t t;
      if (WioCellular.getClock(&t, nullptr) != WioCellularResult::Ok) abort();
      setTime(&t);

      // Send
      if (WioNetwork.waitUntilCommunicationAvailable(NETWORK_TIMEOUT)) {
        while (true) {
          // Read block info from storage
          std::vector<Storage::BlockInfo> blocks;
          if (!Storage::readBlockInfo(&blocks, BATCH_SIZE)) abort();
          Serial.printf(TASK_NAME "Read %d block info from storage\n", blocks.size());
          if (blocks.size() <= 0) break;

          std::string mergedData = "{\"data\":[";
          std::vector<char> data;
          for (int i = 0; i < blocks.size(); ++i) {
            const auto& block = blocks[i];

            // Peek data from storage
            data.resize(block.size + 1);
            if (!Storage::peek(block, data.data(), data.size() - 1, &readDataSize)) abort();
            if (readDataSize != data.size() - 1) abort();
            data[data.size() - 1] = '\0';
            Serial.printf(TASK_NAME "Peek %d bytes from storage %s\n", data.size() - 1, data.data());

            if (i >= 1) mergedData += ',';
            mergedData.append(data.data(), data.size() - 1);
          }
          mergedData += "]}";

          // Send data
          Serial.printf(TASK_NAME "Sending %s\n", mergedData.c_str());
          if (!send(mergedData.data(), mergedData.size())) break;

          // Processed data from storage
          for (int i = 0; i < blocks.size(); ++i) {
            if (!Storage::read(nullptr, 0, nullptr)) abort();
            Serial.println(TASK_NAME "Processed data from storage");
          }
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
    static uint8_t recvData[WioCellular.RECEIVE_SOCKET_SIZE_MAX + 1];
    size_t recvSize;
    if (!client.receive(recvData, sizeof(recvData) - 1, &recvSize, RECEIVE_TIMEOUT)) {
      Serial.printf(TASK_NAME "ERROR: Failed to receive socket %s\n", WioCellularResultToString(client.getLastResult()));
      return false;
    }
    recvData[recvSize] = '\0';

    Serial.printf(TASK_NAME "Received %s\n", recvData);
  }

  return true;
}
