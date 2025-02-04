/*
 * Storage.cpp
 * Copyright (C) Seeed K.K.
 * MIT License
 */

#include <Arduino.h>
#include "Storage.hpp"
#include <cassert>
#include <memory>
#include <Adafruit_SPIFlash.h>

class CriticalSection {
public:
  explicit CriticalSection(SemaphoreHandle_t mutex)
    : Mutex(mutex) {
    assert(Mutex);

    xSemaphoreTake(Mutex, portMAX_DELAY);  // FreeRTOS
  }

  ~CriticalSection(void) {
    xSemaphoreGive(Mutex);  // FreeRTOS
  }

private:
  SemaphoreHandle_t Mutex;  // FreeRTOS
};

static constexpr uint16_t MARKER_ADDRESS = 0;
static constexpr uint16_t WRITE_INDEX_ADDRESS = 4;
static constexpr uint16_t READ_INDEX_ADDRESS = 10;
static constexpr uint16_t DATA_ADDRESS = 16;

static const SPIFlash_Device_t SPIFLASH_DEVICE = FERAM_DEVICE_CONFIG;

static SPIClass FlashSpi(FERAM_SPI, PIN_FERAM_SO, PIN_FERAM_SCK, PIN_FERAM_SI);
static Adafruit_FlashTransport_SPI FlashTransport(PIN_FERAM_CS, FlashSpi);
static Adafruit_SPIFlash Flash(&FlashTransport);

static SemaphoreHandle_t FeramMutex;  // FreeRTOS

static bool FeramWrite(uint16_t address, const void *data, size_t dataSize) {
  assert(address < Flash.size());
  assert(data);
  assert(dataSize >= 1);

  if (nrf_dma_accessible_check(nullptr, data)) {
    if (Flash.writeBuffer(address, reinterpret_cast<const uint8_t *>(data), dataSize) != dataSize) return false;
  } else {
    std::unique_ptr<uint8_t[]> dataInRam = std::make_unique<uint8_t[]>(dataSize);
    memcpy(dataInRam.get(), data, dataSize);
    if (Flash.writeBuffer(address, dataInRam.get(), dataSize) != dataSize) return false;
  }

  // Verify
  uint8_t readData[dataSize];
  assert(Flash.readBuffer(address, readData, dataSize) == dataSize);
  if (memcmp(data, readData, dataSize) != 0) {
    Serial.printf("Verify failed %u %u\n", address, dataSize);
    abort();
  }

  return true;
}

static bool FeramRead(uint16_t address, void *data, size_t dataSize) {
  assert(address < Flash.size());
  assert(data);
  assert(dataSize >= 1);

  if (Flash.readBuffer(address, reinterpret_cast<uint8_t *>(data), dataSize) != dataSize) return false;

  return true;
}

static uint16_t DataMaxSize(void) {
  return Flash.size() - DATA_ADDRESS;
}

static uint16_t DataFreeSize(uint16_t writeIndex, uint16_t readIndex) {
  assert(writeIndex < DataMaxSize());
  assert(readIndex < DataMaxSize());

  uint16_t dataFreeSize = writeIndex < readIndex ? readIndex - writeIndex : readIndex + DataMaxSize() - writeIndex;
  if (dataFreeSize >= 1) --dataFreeSize;

  return dataFreeSize;
}

static bool GetMarker(uint32_t *marker) {
  assert(marker);

  if (!FeramRead(MARKER_ADDRESS, marker, sizeof(*marker))) return false;

  return true;
}

static bool SetMarker(uint32_t marker) {
  if (!FeramWrite(MARKER_ADDRESS, &marker, sizeof(marker))) return false;

  return true;
}

static bool GetIndex(uint16_t *writeIndex, uint16_t *readIndex) {
  assert(writeIndex);
  assert(readIndex);

  uint16_t index[3 * 2];
  if (!FeramRead(WRITE_INDEX_ADDRESS, index, sizeof(index))) return false;

  if (index[1] == index[2] || index[0] == index[1])
    *writeIndex = index[1];
  else
    *writeIndex = index[0];

  if (index[4] == index[5] || index[3] == index[4])
    *readIndex = index[4];
  else
    *readIndex = index[3];

  return true;
}

static bool SetWriteIndex(uint16_t writeIndex) {
  assert(writeIndex < DataMaxSize());

  uint16_t index[3] = { writeIndex, writeIndex, writeIndex };
  if (!FeramWrite(WRITE_INDEX_ADDRESS, index, sizeof(index))) return false;

  return true;
}

static bool SetReadIndex(uint16_t readIndex) {
  assert(readIndex < DataMaxSize());

  uint16_t index[3] = { readIndex, readIndex, readIndex };
  if (!FeramWrite(READ_INDEX_ADDRESS, index, sizeof(index))) return false;

  return true;
}

static bool SetData(uint16_t index, const void *data, uint16_t dataSize) {
  assert(index < DataMaxSize());
  assert(data);
  assert(dataSize >= 1);

  uint16_t firstDataSize = std::min(dataSize, static_cast<uint16_t>(DataMaxSize() - index));
  if (!FeramWrite(DATA_ADDRESS + index, data, firstDataSize)) return false;
  if (dataSize > firstDataSize) {
    if (!FeramWrite(DATA_ADDRESS, reinterpret_cast<const uint8_t *>(data) + firstDataSize, dataSize - firstDataSize)) return false;
  }

  return true;
}

static bool GetData(uint16_t index, void *data, uint16_t dataSize) {
  assert(index < DataMaxSize());
  assert(data);
  assert(dataSize >= 1);

  uint16_t firstDataSize = std::min(dataSize, static_cast<uint16_t>(DataMaxSize() - index));
  if (!FeramRead(DATA_ADDRESS + index, data, firstDataSize)) return false;
  if (dataSize > firstDataSize) {
    if (!FeramRead(DATA_ADDRESS, reinterpret_cast<uint8_t *>(data) + firstDataSize, dataSize - firstDataSize)) return false;
  }

  return true;
}

bool Storage::begin(void) {
  digitalWrite(PIN_FERAM_WP, HIGH);
  digitalWrite(PIN_FERAM_HOLD, HIGH);
  pinMode(PIN_FERAM_WP, OUTPUT);
  pinMode(PIN_FERAM_HOLD, OUTPUT);

  if (!Flash.begin(&SPIFLASH_DEVICE, 1)) return false;

  FeramMutex = xSemaphoreCreateMutex();  // FreeRTOS
  assert(FeramMutex);

  return true;
}

bool Storage::clear(void) {
  std::unique_ptr<CriticalSection> cs = std::make_unique<CriticalSection>(FeramMutex);

  if (!SetMarker(0)) return false;

  if (!SetWriteIndex(0)) return false;
  if (!SetReadIndex(0)) return false;

  uint8_t data = 0;
  for (uint16_t i = 0; i < DataMaxSize(); ++i) {
    if (!SetData(i, &data, 1)) return false;
  }

  return true;
}

bool Storage::writeMarker(uint32_t marker) {
  std::unique_ptr<CriticalSection> cs = std::make_unique<CriticalSection>(FeramMutex);

  return SetMarker(marker);
}

bool Storage::readMarker(uint32_t *marker) {
  assert(marker);
  std::unique_ptr<CriticalSection> cs = std::make_unique<CriticalSection>(FeramMutex);

  return GetMarker(marker);
}

bool Storage::freeSize(uint16_t *size) {
  std::unique_ptr<CriticalSection> cs = std::make_unique<CriticalSection>(FeramMutex);

  uint16_t writeIndex;
  uint16_t readIndex;
  if (!GetIndex(&writeIndex, &readIndex)) return false;
  *size = DataFreeSize(writeIndex, readIndex);

  return true;
}

bool Storage::write(const void *data, uint16_t dataSize) {
  assert(data);
  assert(dataSize >= 1);
  std::unique_ptr<CriticalSection> cs = std::make_unique<CriticalSection>(FeramMutex);

  uint16_t writeIndex;
  uint16_t readIndex;
  if (!GetIndex(&writeIndex, &readIndex)) return false;
  if (DataFreeSize(writeIndex, readIndex) < 2 + dataSize) return false;

  if (!SetData(writeIndex, &dataSize, sizeof(dataSize))) return false;
  if (dataSize >= 1) {
    if (!SetData((writeIndex + 2) % DataMaxSize(), data, dataSize)) return false;
  }

  if (!SetWriteIndex((writeIndex + 2 + dataSize) % DataMaxSize())) return false;

  return true;
}

bool Storage::peek(void *data, uint16_t dataSize, uint16_t *readDataSize) {
  assert((data && dataSize >= 1) || (!data && dataSize == 0));
  assert(readDataSize);
  std::unique_ptr<CriticalSection> cs = std::make_unique<CriticalSection>(FeramMutex);

  uint16_t writeIndex;
  uint16_t readIndex;
  if (!GetIndex(&writeIndex, &readIndex)) return false;
  if (readIndex == writeIndex) {
    *readDataSize = 0;
    return true;
  }

  if (!GetData(readIndex, readDataSize, sizeof(*readDataSize))) return false;
  if (*readDataSize <= 0) return false;
  if (data && dataSize >= 1 && *readDataSize >= 1) {
    if (*readDataSize > dataSize) return false;
    if (!GetData((readIndex + 2) % DataMaxSize(), data, *readDataSize)) return false;
  }

  return true;
}

bool Storage::read(void *data, uint16_t dataSize, uint16_t *readDataSize) {
  assert((data && dataSize >= 1) || (!data && dataSize == 0));
  assert(readDataSize);
  std::unique_ptr<CriticalSection> cs = std::make_unique<CriticalSection>(FeramMutex);

  uint16_t writeIndex;
  uint16_t readIndex;
  if (!GetIndex(&writeIndex, &readIndex)) return false;
  if (readIndex == writeIndex) {
    *readDataSize = 0;
    return true;
  }

  if (!GetData(readIndex, readDataSize, sizeof(*readDataSize))) return false;
  if (*readDataSize <= 0) return false;
  if (data && dataSize >= 1 && *readDataSize >= 1) {
    if (*readDataSize > dataSize) return false;
    if (!GetData((readIndex + 2) % DataMaxSize(), data, *readDataSize)) return false;
  }

  if (!SetReadIndex((readIndex + 2 + *readDataSize) % DataMaxSize())) return false;

  return true;
}
