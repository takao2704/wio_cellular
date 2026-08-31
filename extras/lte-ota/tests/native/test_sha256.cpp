#include <cassert>
#include <cstdint>
#include <cstring>

#include "WioOtaSha256.h"

namespace {

uint8_t fromHex(char value) {
  if (value >= '0' && value <= '9') return value - '0';
  return value - 'a' + 10;
}

void decode(const char* hex, uint8_t output[wio_ota::kSha256Size]) {
  for (size_t i = 0; i < wio_ota::kSha256Size; ++i) {
    output[i] = static_cast<uint8_t>((fromHex(hex[i * 2]) << 4) |
                                     fromHex(hex[i * 2 + 1]));
  }
}

}  // namespace

int main() {
  constexpr char kExpectedHex[] =
      "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad";
  uint8_t expected[wio_ota::kSha256Size];
  decode(kExpectedHex, expected);

  uint8_t actual[wio_ota::kSha256Size];
  wio_ota::Sha256 one_shot;
  one_shot.update(reinterpret_cast<const uint8_t*>("abc"), 3);
  one_shot.finish(actual);
  assert(std::memcmp(actual, expected, sizeof(actual)) == 0);

  wio_ota::Sha256 chunked;
  chunked.update(reinterpret_cast<const uint8_t*>("a"), 1);
  chunked.update(reinterpret_cast<const uint8_t*>("bc"), 2);
  chunked.finish(actual);
  assert(std::memcmp(actual, expected, sizeof(actual)) == 0);
  return 0;
}
