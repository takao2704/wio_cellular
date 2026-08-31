#pragma once

#include <stddef.h>
#include <stdint.h>

namespace wio_ota {

constexpr size_t kSha256Size = 32;

class Sha256 {
 public:
  Sha256();

  void reset();
  void update(const uint8_t* data, size_t size);
  void finish(uint8_t output[kSha256Size]);

 private:
  void transform(const uint8_t block[64]);

  uint32_t state_[8];
  uint64_t total_size_;
  uint8_t buffer_[64];
  size_t buffer_size_;
};

}  // namespace wio_ota
