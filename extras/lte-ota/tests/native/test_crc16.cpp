#include <cassert>
#include <cstdint>
#include <cstring>

#include "WioOtaCrc16.h"

int main() {
  const auto* input = reinterpret_cast<const uint8_t*>("123456789");
  assert(wio_ota::crc16Ccitt(input, 9) == 0x29b1);

  uint16_t chunked = wio_ota::crc16Ccitt(input, 4);
  chunked = wio_ota::crc16Ccitt(input + 4, 5, chunked);
  assert(chunked == 0x29b1);

  assert(wio_ota::crc16Ccitt(nullptr, 0) == 0xffff);
  return 0;
}
