#include "WioOtaVersionStore.h"

#include <Adafruit_LittleFS.h>
#include <InternalFileSystem.h>
#include <WioOtaCrc16.h>

#include <cstddef>

namespace wio_ota_agent {
namespace {

using Adafruit_LittleFS_Namespace::FILE_O_READ;
using Adafruit_LittleFS_Namespace::FILE_O_WRITE;
using Adafruit_LittleFS_Namespace::File;

constexpr char kSlotPaths[][24] = {"/wio-ota-version-a.bin",
                                    "/wio-ota-version-b.bin"};
constexpr uint32_t kRecordMagic = 0x574f5631u;  // "WOV1"

struct VersionRecord {
  uint32_t magic;
  uint32_t version;
  uint32_t version_inverse;
  uint16_t crc16;
  uint16_t reserved;
};

static_assert(sizeof(VersionRecord) == 16, "unexpected version record size");

uint16_t recordCrc(const VersionRecord& record) {
  return wio_ota::crc16Ccitt(
      reinterpret_cast<const uint8_t*>(&record),
      offsetof(VersionRecord, crc16));
}

bool validRecord(const VersionRecord& record) {
  return record.magic == kRecordMagic &&
         record.version_inverse == ~record.version &&
         record.crc16 == recordCrc(record);
}

bool readRecord(const char* path, VersionRecord* record) {
  File file(InternalFS);
  if (!file.open(path, FILE_O_READ)) {
    return false;
  }
  const size_t received = file.read(record, sizeof(*record));
  file.close();
  return received == sizeof(*record) && validRecord(*record);
}

bool writeRecord(const char* path, uint32_t version) {
  VersionRecord record = {kRecordMagic, version, ~version, 0, 0};
  record.crc16 = recordCrc(record);

  if (InternalFS.exists(path) && !InternalFS.remove(path)) {
    return false;
  }
  File file(InternalFS);
  if (!file.open(path, FILE_O_WRITE)) {
    return false;
  }
  const size_t written = file.write(
      reinterpret_cast<const uint8_t*>(&record), sizeof(record));
  file.close();
  return written == sizeof(record);
}

}  // namespace

const char* versionStoreErrorString(VersionStoreError error) {
  switch (error) {
    case VersionStoreError::kNone:
      return "none";
    case VersionStoreError::kMountFailed:
      return "internal filesystem mount failed";
    case VersionStoreError::kWriteFailed:
      return "version record write failed";
    case VersionStoreError::kReadBackFailed:
      return "version record read-back failed";
  }
  return "unknown";
}

bool VersionStore::begin() {
  last_error_ = VersionStoreError::kNone;
  highest_version_ = 0;
  next_slot_ = 0;
  if (!InternalFS.begin()) {
    last_error_ = VersionStoreError::kMountFailed;
    return false;
  }

  VersionRecord records[2] = {};
  const bool valid[2] = {readRecord(kSlotPaths[0], &records[0]),
                         readRecord(kSlotPaths[1], &records[1])};
  if (valid[0] && (!valid[1] || records[0].version >= records[1].version)) {
    highest_version_ = records[0].version;
    next_slot_ = 1;
  } else if (valid[1]) {
    highest_version_ = records[1].version;
    next_slot_ = 0;
  }
  return true;
}

bool VersionStore::recordCurrentVersion(uint32_t version) {
  last_error_ = VersionStoreError::kNone;
  if (version <= highest_version_) {
    return true;
  }
  if (!writeRecord(kSlotPaths[next_slot_], version)) {
    last_error_ = VersionStoreError::kWriteFailed;
    return false;
  }
  VersionRecord verified = {};
  if (!readRecord(kSlotPaths[next_slot_], &verified) ||
      verified.version != version) {
    last_error_ = VersionStoreError::kReadBackFailed;
    return false;
  }
  highest_version_ = version;
  next_slot_ ^= 1u;
  return true;
}

}  // namespace wio_ota_agent
