#pragma once

#include <stddef.h>
#include <stdint.h>

namespace wio_ota {

constexpr uint16_t kCrc16InitialValue = 0xffff;

uint16_t crc16Ccitt(const uint8_t* data, size_t size,
                    uint16_t previous = kCrc16InitialValue);

}  // namespace wio_ota
