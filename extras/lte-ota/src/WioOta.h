#pragma once

#include <stddef.h>
#include <stdint.h>

#include "WioOtaImageVerifier.h"

namespace wio_ota {

constexpr uint32_t kApplicationAddress = 0x00027000;
constexpr uint32_t kBank1Address = 0x00088000;
constexpr uint32_t kMaximumImageSize = 397312;
constexpr uint32_t kBootloaderAddress = 0x000f4000;
constexpr uint32_t kBootloaderSettingsAddress = 0x000ff000;
constexpr uint32_t kFlashPageSize = 4096;

enum class Error {
  kNone,
  kInvalidState,
  kInvalidArgument,
  kImageTooLarge,
  kSoftDeviceEnabled,
  kFlashEraseFailed,
  kFlashWriteFailed,
  kFlashVerifyFailed,
  kImageIncomplete,
  kCrcMismatch,
  kSha256Mismatch,
  kCrcDisabledValue,
  kInvalidVectorTable,
  kIncompatibleBootloaderSettings,
};

const char* errorString(Error error);

class Writer {
 public:
  Writer();

  Error begin(size_t image_size);
  Error write(const uint8_t* data, size_t size);
  Error finish(uint16_t expected_crc,
               const uint8_t expected_sha256[kSha256Size]);
  Error activate();
  void discard();
  void resetToApply() const;

  size_t bytesWritten() const { return verifier_.bytesReceived(); }
  size_t imageSize() const { return verifier_.imageSize(); }
  uint16_t calculatedCrc() const { return verifier_.calculatedCrc(); }
  const uint8_t* calculatedSha256() const {
    return verifier_.calculatedSha256();
  }
  bool isVerified() const { return state_ == State::kVerified; }
  bool isActivated() const { return state_ == State::kActivated; }

 private:
  enum class State { kIdle, kReceiving, kVerified, kActivated, kFailed };

  Error fail(Error error);
  Error requireSoftDeviceDisabled() const;
  Error validateVectorTable() const;
  Error eraseImagePages(size_t image_size);
  Error writeAndVerify(uint32_t address, const void* data, size_t size);

  State state_;
  ImageVerifier verifier_;
};

}  // namespace wio_ota
