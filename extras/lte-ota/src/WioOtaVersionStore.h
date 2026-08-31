#pragma once

#include <stdint.h>

namespace wio_ota_agent {

enum class VersionStoreError {
  kNone,
  kMountFailed,
  kWriteFailed,
  kReadBackFailed,
};

const char* versionStoreErrorString(VersionStoreError error);

// Stores the highest version that has reached application setup(). The two
// alternating records ensure that one valid record remains if power is lost
// while the other record is being replaced.
class VersionStore {
 public:
  bool begin();
  bool recordCurrentVersion(uint32_t version);

  uint32_t highestInstalledVersion() const { return highest_version_; }
  VersionStoreError lastError() const { return last_error_; }

 private:
  uint32_t highest_version_ = 0;
  uint8_t next_slot_ = 0;
  VersionStoreError last_error_ = VersionStoreError::kNone;
};

}  // namespace wio_ota_agent
