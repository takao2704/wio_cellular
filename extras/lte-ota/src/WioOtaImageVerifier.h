#pragma once

#include <stddef.h>
#include <stdint.h>

#include "WioOtaSha256.h"

namespace wio_ota {

enum class VerificationError {
  kNone,
  kInvalidState,
  kInvalidArgument,
  kImageIncomplete,
  kCrcMismatch,
  kSha256Mismatch,
  kCrcDisabledValue,
};

const char* verificationErrorString(VerificationError error);

class ImageVerifier {
 public:
  ImageVerifier();

  VerificationError begin(size_t image_size);
  VerificationError update(const uint8_t* data, size_t size);
  VerificationError finish(
      uint16_t expected_crc,
      const uint8_t expected_sha256[kSha256Size]);
  void reset();

  size_t bytesReceived() const { return bytes_received_; }
  size_t imageSize() const { return image_size_; }
  uint16_t calculatedCrc() const { return crc_; }
  const uint8_t* calculatedSha256() const { return sha256_; }
  bool isVerified() const { return state_ == State::kVerified; }

 private:
  enum class State { kIdle, kReceiving, kVerified, kFailed };

  VerificationError fail(VerificationError error);

  State state_;
  size_t image_size_;
  size_t bytes_received_;
  uint16_t crc_;
  Sha256 sha_;
  uint8_t sha256_[kSha256Size];
};

}  // namespace wio_ota
