/*
 * Storage.hpp
 * Copyright (C) Seeed K.K.
 * MIT License
 */

#ifndef STORAGE_HPP
#define STORAGE_HPP

class Storage {
public:
  static bool begin(void);
  static bool clear(void);
  static bool writeMarker(uint32_t marker);
  static bool readMarker(uint32_t *marker);
  static bool freeSize(uint16_t *size);
  static bool write(const void *data, uint16_t dataSize);
  static bool peek(void *data, uint16_t dataSize, uint16_t *readDataSize);
  static bool read(void *data, uint16_t dataSize, uint16_t *readDataSize);
};

#endif  // STORAGE_HPP
