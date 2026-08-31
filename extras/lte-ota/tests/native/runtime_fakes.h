#pragma once

// Host-only boundaries for the real Agent, HTTP, Writer and VersionStore.
// The test runner exposes this header under the Core's include names; it is
// never on an Arduino/PlatformIO include path or in the Arduino library ZIP.
#include <algorithm>
#include <cassert>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <deque>
#include <map>
#include <string>
#include <vector>

using std::min;
inline uint32_t test_clock = 0;
inline uint32_t millis() { return test_clock++; }
inline void delay(uint32_t duration) { test_clock += duration; }
struct Stream {
  std::vector<std::string> lines;
  void println(const char* line) { lines.emplace_back(line); }
  void printf(const char* format, ...) {
    char buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    lines.emplace_back(buffer);
  }
  void flush() {}
};
inline Stream Serial;

namespace test_flash {
alignas(4) inline uint8_t bytes[0x80000];
inline bool softdevice = false;
inline bool erase_fails = false;
inline bool write_fails = false;
inline unsigned erases = 0, writes = 0, resets = 0;
}
inline void* wioOtaTestFlashAddress(uint32_t address) {
  assert(address >= 0x80000 && address < 0x100000);
  return test_flash::bytes + address - 0x80000;
}
constexpr uint32_t NRF_SUCCESS = 0, NRFX_SUCCESS = 0;
inline uint32_t sd_softdevice_is_enabled(uint8_t* enabled) {
  *enabled = test_flash::softdevice;
  return NRF_SUCCESS;
}
inline int nrfx_nvmc_page_erase(uint32_t address) {
  ++test_flash::erases;
  if (test_flash::erase_fails) return 1;
  std::memset(wioOtaTestFlashAddress(address), 0xff, 4096);
  return NRFX_SUCCESS;
}
inline void nrfx_nvmc_bytes_write(uint32_t address, const void* data, size_t size) {
  ++test_flash::writes;
  if (!test_flash::write_fails)
    std::memcpy(wioOtaTestFlashAddress(address), data, size);
}
inline void NVIC_SystemReset() { ++test_flash::resets; }

enum class WioCellularResult {
  Ok,
  WaitCommandTimeout,
  ReadResponseTimeout,
  CommandRejected,
  RdyTimeout,
  OpenTimeout,
  OpenError,
  ReceiveTimeout,
  NotActivate,
  ArgumentOutOfRange,
  InvalidOperation,
  InsufficientResources,
  ConnectTimeout,
  ConnectError,
  Closing,
  Error,
};
struct WioCellularModule {
  std::string manifest, url, echo_failure, execute_failure;
  std::vector<uint8_t> firmware, body;
  std::deque<std::string> responses;
  size_t offset = 0;
  unsigned firmware_gets = 0, closes = 0, deletes = 0, power_offs = 0;
  bool binary_failure = false, close_failure = false, power_off_failure = false;
  bool close_timeout_once = false;
  int delete_timeout_after = 0;
  bool short_file_read = false;
  int delete_failure_after = 0;
  bool omit_url_prompt = false, reject_url_prompt = false;
  bool reject_url = false, omit_url_result = false;
  bool omit_get_result = false, get_failure = false;
  bool delayed_get_result_once = false;
  bool invalid_content_length = false, omit_read_prompt = false;
  bool reject_read_prompt = false;
  bool omit_read_result = false, read_failure = false;
  bool delayed_read_result_once = false;
  bool store_timeout_once = false, store_failure = false;
  bool file_open_failure = false;
  bool file_open_timeout_once = false;
  WioCellularResult execute_failure_result =
      WioCellularResult::CommandRejected;
  int delayed_command_reads = 0, delayed_get_reads = 0;
  int delayed_read_reads = 0;
  int delayed_store_reads = 0;

  WioCellularResult executeCommand(const std::string& command, int) {
    if (command.find("AT+QFCLOSE=") == 0) {
      ++closes;
      if (close_timeout_once) {
        close_timeout_once = false;
        delayed_command_reads = 5;
        return WioCellularResult::ReadResponseTimeout;
      }
      if (close_failure) return WioCellularResult::CommandRejected;
    }
    if (command.find("AT+QFDEL=") == 0) {
      ++deletes;
      if (delete_timeout_after > 0) {
        --delete_timeout_after;
        if (delete_timeout_after == 0) {
          delayed_command_reads = 5;
          return WioCellularResult::ReadResponseTimeout;
        }
      }
      if (delete_failure_after > 0) {
        --delete_failure_after;
        if (delete_failure_after == 0) {
          return WioCellularResult::CommandRejected;
        }
      }
    }
    if (!execute_failure.empty() &&
        command.find(execute_failure) == 0) {
      if (execute_failure_result == WioCellularResult::WaitCommandTimeout ||
          execute_failure_result == WioCellularResult::ReadResponseTimeout) {
        delayed_command_reads = 5;
      }
      return execute_failure_result;
    }
    return WioCellularResult::Ok;
  }
  bool writeAndWaitCommand(const std::string& command, uint32_t) {
    responses.clear();
    if (!echo_failure.empty() && command.find(echo_failure) == 0) return false;
    if (command.find("AT+QHTTPURL=") == 0) {
      if (reject_url_prompt) responses = {"ERROR"};
      else if (!omit_url_prompt) responses = {"CONNECT"};
    }
    else if (command == "AT+QHTTPGET=80") {
      if (url.find("/v1/userdata") != std::string::npos)
        body.assign(manifest.begin(), manifest.end());
      else { body = firmware; ++firmware_gets; }
      offset = 0;
      responses = {"OK"};
      if (delayed_get_result_once) {
        delayed_get_result_once = false;
        delayed_get_reads = 95;
      } else if (!omit_get_result) {
        responses.push_back(
            "+QHTTPGET: " + std::to_string(get_failure ? 1 : 0) +
            ",200," +
            std::to_string(invalid_content_length ? -1 : int(body.size())));
      }
    } else if (command == "AT+QHTTPREAD=80") {
      if (reject_read_prompt) {
        responses = {"ERROR"};
      } else {
        if (!omit_read_prompt) responses = {"CONNECT"};
        responses.push_back("OK");
        if (delayed_read_result_once) {
          delayed_read_result_once = false;
          delayed_read_reads = 15;
        } else if (!omit_read_result) {
          responses.push_back(read_failure ? "+QHTTPREAD: 1"
                                           : "+QHTTPREAD: 0");
        }
      }
    } else if (command.find("AT+QHTTPREADFILE=") == 0) {
      responses = {"OK"};
      if (store_timeout_once) {
        store_timeout_once = false;
        delayed_store_reads = 95;
      } else {
        responses.push_back(store_failure ? "+QHTTPREADFILE: 1"
                                          : "+QHTTPREADFILE: 0");
      }
    }
    else if (command.find("AT+QFREAD=") == 0) {
      size_t count = std::stoul(command.substr(command.find(',') + 1));
      responses = {"CONNECT " + std::to_string(short_file_read ? count - 1 : count), "OK"};
    } else assert(false && "unexpected AT command");
    return true;
  }
  void writeBinary(const void* bytes, size_t size) {
    url.assign(static_cast<const char*>(bytes), size);
    if (!omit_url_result) responses.emplace_back(reject_url ? "ERROR" : "OK");
  }
  std::string readResponse(uint32_t timeout) {
    if (responses.empty()) {
      if (delayed_command_reads > 0) {
        --delayed_command_reads;
        if (delayed_command_reads == 0) return "OK";
      }
      if (delayed_get_reads > 0) {
        --delayed_get_reads;
        if (delayed_get_reads == 0) {
          return "+QHTTPGET: 0,200," + std::to_string(body.size());
        }
      }
      if (delayed_read_reads > 0) {
        --delayed_read_reads;
        if (delayed_read_reads == 0) return "+QHTTPREAD: 0";
      }
      if (delayed_store_reads > 0) {
        --delayed_store_reads;
        if (delayed_store_reads == 0) return "+QHTTPREADFILE: 0";
      }
      test_clock += timeout;
      return {};
    }
    auto response = responses.front(); responses.pop_front(); return response;
  }
  bool readBinary(void* output, size_t size, uint32_t) {
    if ((binary_failure && url.find("/v1/userdata") == std::string::npos) ||
        offset + size > body.size()) return false;
    std::memcpy(output, body.data() + offset, size); offset += size; return true;
  }
  template <typename Callback>
  WioCellularResult queryCommand(const std::string&, Callback callback, int) {
    if (file_open_timeout_once) {
      file_open_timeout_once = false;
      delayed_command_reads = 5;
      return WioCellularResult::ReadResponseTimeout;
    }
    if (file_open_failure) return WioCellularResult::OpenError;
    return callback("+QFOPEN: 1") ? WioCellularResult::Ok : WioCellularResult::Error;
  }
  WioCellularResult powerOff() {
    ++power_offs;
    if (power_off_failure) return WioCellularResult::Error;
    responses.clear();
    delayed_command_reads = delayed_get_reads = delayed_read_reads = 0;
    delayed_store_reads = 0;
    offset = 0;
    return WioCellularResult::Ok;
  }
};

constexpr int LFS_ERR_NOENT = -2, LFS_ERR_IO = -5, LFS_ERR_CORRUPT = -84;
constexpr uint8_t LFS_TYPE_REG = 1, LFS_TYPE_DIR = 2;
struct lfs_info { uint8_t type; uint32_t size; };
struct lfs_t {};
namespace test_fs {
inline std::map<std::string, std::vector<uint8_t>> files;
inline std::map<std::string, int> stat_errors;
inline std::string directory_path;
inline std::string open_failure, read_failure;
inline bool mount_ok = true, remove_ok = true, short_write = false;
inline bool corrupt_write = false;
inline unsigned formats = 0, writes = 0, removes = 0, locks = 0, open_files = 0;
inline void reset() {
  files.clear(); stat_errors.clear(); open_failure.clear(); read_failure.clear(); directory_path.clear();
  mount_ok = remove_ok = true; short_write = corrupt_write = false;
  formats = writes = removes = locks = open_files = 0;
}
}
inline int lfs_stat(lfs_t*, const char* path, lfs_info* info) {
  if (test_fs::stat_errors.count(path)) return test_fs::stat_errors[path];
  if (test_fs::directory_path == path) { info->type = LFS_TYPE_DIR; info->size = 16; return 0; }
  auto entry = test_fs::files.find(path);
  if (entry == test_fs::files.end()) return LFS_ERR_NOENT;
  info->type = LFS_TYPE_REG;
  info->size = static_cast<uint32_t>(entry->second.size());
  return 0;
}
class Adafruit_LittleFS {
 public:
  bool begin() { return test_fs::mount_ok; }
};
class InternalFileSystem : public Adafruit_LittleFS {
 public:
  // Match the dangerous Core behavior so tests detect accidentally using it.
  bool begin() {
    if (!test_fs::mount_ok) {
      ++test_fs::formats; test_fs::files.clear(); test_fs::mount_ok = true;
    }
    return true;
  }
  bool exists(const char* path) {
    lfs_info info{}; return lfs_stat(&fs_, path, &info) == 0;
  }
  bool remove(const char* path) {
    ++test_fs::removes;
    if (!test_fs::remove_ok) return false;
    return test_fs::files.erase(path) != 0;
  }
  lfs_t* _getFS() { return &fs_; }
  void _lockFS() { ++test_fs::locks; }
  void _unlockFS() { assert(test_fs::locks); --test_fs::locks; }
 private:
  lfs_t fs_;
};
inline InternalFileSystem InternalFS;
namespace Adafruit_LittleFS_Namespace {
constexpr uint8_t FILE_O_READ = 0, FILE_O_WRITE = 1;
class File {
 public:
  explicit File(InternalFileSystem&) {}
  bool open(const char* path, uint8_t mode) {
    if (test_fs::open_failure == path) return false;
    if (mode == FILE_O_READ && !test_fs::files.count(path)) return false;
    path_ = path; ++test_fs::open_files; return true;
  }
  int read(void* output, size_t size) {
    if (test_fs::read_failure == path_) return -1;
    const auto& bytes = test_fs::files.at(path_);
    size = std::min(size, bytes.size()); std::memcpy(output, bytes.data(), size); return size;
  }
  size_t write(const uint8_t* bytes, size_t size) {
    ++test_fs::writes;
    if (test_fs::short_write) --size;
    test_fs::files[path_].assign(bytes, bytes + size);
    if (test_fs::corrupt_write && size) test_fs::files[path_][0] ^= 1;
    return size;
  }
  void close() { assert(test_fs::open_files); --test_fs::open_files; }
 private:
  std::string path_;
};
}
