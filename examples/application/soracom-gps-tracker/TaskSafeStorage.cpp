/*
 * TaskSafeStorage.cpp
 * Copyright (C) Seeed K.K.
 * MIT License
 */

#include <Arduino.h>
#include "TaskSafeStorage.hpp"
#include <memory>

using namespace wiocellular::component::nonvolatilememory;

static const SPIFlash_Device_t SPIFLASH_DEVICE = FERAM_DEVICE_CONFIG;

static SPIClass FlashSpi(FERAM_SPI, PIN_FERAM_SO, PIN_FERAM_SCK, PIN_FERAM_SI);
static Adafruit_FlashTransport_SPI FlashTransport(PIN_FERAM_CS, FlashSpi);
static Adafruit_SPIFlash Flash(&FlashTransport);
static Adafruit_SPIFlashMemory FlashMemory(Flash);

static NonVolatileMemory NvmMarker(FlashMemory, 0x0000, 4);
static NonVolatileMemory NvmAbnormalRebootCount(FlashMemory, 0x0004, 1);
static NonVolatileMemory NvmSendBuffer(FlashMemory, 0x0400, 0x10000 - 0x400);
static NonVolatileBlockQueue NvmSendQueue(NvmSendBuffer);

static SemaphoreHandle_t FeramMutex;  // FreeRTOS

void TaskSafeStorage::begin() {
  digitalWrite(PIN_FERAM_WP, HIGH);
  digitalWrite(PIN_FERAM_HOLD, HIGH);
  pinMode(PIN_FERAM_WP, OUTPUT);
  pinMode(PIN_FERAM_HOLD, OUTPUT);

  if (!Flash.begin(&SPIFLASH_DEVICE, 1)) abort();

  FeramMutex = xSemaphoreCreateMutex();  // FreeRTOS
  assert(FeramMutex);
}

void TaskSafeStorage::clear() {
  std::unique_ptr<CriticalSection> cs = std::make_unique<CriticalSection>(FeramMutex);

  NvmMarker.value<uint32_t>() = 0;
  NvmAbnormalRebootCount.value<uint8_t>() = 0;
  if (NvmSendQueue.clear() < 0) abort();
}

void TaskSafeStorage::writeMarker(uint32_t marker) {
  std::unique_ptr<CriticalSection> cs = std::make_unique<CriticalSection>(FeramMutex);

  NvmMarker.value<uint32_t>() = marker;
}

uint32_t TaskSafeStorage::readMarker() {
  std::unique_ptr<CriticalSection> cs = std::make_unique<CriticalSection>(FeramMutex);

  return NvmMarker.value<uint32_t>();
}

void TaskSafeStorage::writeAbnormalRebootCount(uint8_t count) {
  std::unique_ptr<CriticalSection> cs = std::make_unique<CriticalSection>(FeramMutex);

  NvmAbnormalRebootCount.value<uint8_t>() = count;
}

uint8_t TaskSafeStorage::readAbnormalRebootCount() {
  std::unique_ptr<CriticalSection> cs = std::make_unique<CriticalSection>(FeramMutex);

  return NvmAbnormalRebootCount.value<uint8_t>();
}

size_t TaskSafeStorage::SendQueue::freeSize() {
  std::unique_ptr<CriticalSection> cs = std::make_unique<CriticalSection>(FeramMutex);

  size_t size;
  if (NvmSendQueue.freeSize(&size) < 0) abort();

  return size;
}

bool TaskSafeStorage::SendQueue::write(const void *data, size_t dataSize) {
  std::unique_ptr<CriticalSection> cs = std::make_unique<CriticalSection>(FeramMutex);

  return NvmSendQueue.write(data, dataSize) >= 0;
}

bool TaskSafeStorage::SendQueue::available() {
  std::unique_ptr<CriticalSection> cs = std::make_unique<CriticalSection>(FeramMutex);

  size_t readDataSize;
  if (NvmSendQueue.peek(nullptr, 0, &readDataSize) < 0) abort();

  return readDataSize >= 1;
}

void TaskSafeStorage::SendQueue::peekBlockInfo(std::vector<NonVolatileBlockQueue::BlockInfo> *blockInfoList, size_t maxSize) {
  std::unique_ptr<CriticalSection> cs = std::make_unique<CriticalSection>(FeramMutex);

  if (NvmSendQueue.peekBlockInfo(blockInfoList, maxSize) < 0) abort();
}

void TaskSafeStorage::SendQueue::peek(const NonVolatileBlockQueue::BlockInfo &blockInfo, void *data, size_t dataSize, size_t *readDataSize) {
  std::unique_ptr<CriticalSection> cs = std::make_unique<CriticalSection>(FeramMutex);

  if (NvmSendQueue.peek(blockInfo, data, dataSize, readDataSize) < 0) abort();
}

void TaskSafeStorage::SendQueue::read(void *data, size_t dataSize, size_t *readDataSize) {
  std::unique_ptr<CriticalSection> cs = std::make_unique<CriticalSection>(FeramMutex);

  if (NvmSendQueue.read(data, dataSize, readDataSize) < 0) abort();
}
