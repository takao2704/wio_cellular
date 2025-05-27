/*
 * TaskSafeStorage.hpp
 * Copyright (C) Seeed K.K.
 * MIT License
 */

#ifndef TASKSAFESTORAGE_HPP
#define TASKSAFESTORAGE_HPP

#include <cassert>
#include <vector>
#include <semphr.h>  // FreeRTOS
#include <Adafruit_SPIFlash.h>
#include <WioNonVolatileMemory.h>

class CriticalSection {
public:
  explicit CriticalSection(SemaphoreHandle_t mutex)
    : Mutex(mutex) {
    assert(Mutex);

    xSemaphoreTake(Mutex, portMAX_DELAY);  // FreeRTOS
  }

  ~CriticalSection() {
    xSemaphoreGive(Mutex);  // FreeRTOS
  }

private:
  SemaphoreHandle_t Mutex;  // FreeRTOS
};

class TaskSafeStorage {
public:
  TaskSafeStorage() = delete;

  static void begin();
  static void clear();

  static void writeMarker(uint32_t marker);
  static uint32_t readMarker();


  class SendQueue {
  public:
    SendQueue() = delete;

    static size_t freeSize();
    static bool write(const void *data, size_t dataSize);
    static bool available();
    static void peekBlockInfo(std::vector<wiocellular::component::nonvolatilememory::NonVolatileBlockQueue::BlockInfo> *BlockInfoList, size_t maxSize = SIZE_MAX);
    static void peek(const wiocellular::component::nonvolatilememory::NonVolatileBlockQueue::BlockInfo &blockInfo, void *data, size_t dataSize, size_t *readDataSize);
    static void read(void *data, size_t dataSize, size_t *readDataSize);
  };
};

#endif  // TASKSAFESTORAGE_HPP
