#pragma once

#include <Arduino.h>
#include <WioCellular.h>

#include <functional>
#include <string>

namespace wio_bg770a_http {

struct Response {
  int result = -1;
  int status_code = -1;
  int content_length = -1;
};

enum class Error {
  kNone,
  kConfigurationRejected,
  kCommandEchoTimeout,
  kPromptTimeout,
  kCommandRejected,
  kGetResultTimeout,
  kGetFailed,
  kInvalidContentLength,
  kBodyTimeout,
  kBodySinkRejected,
  kReadResultTimeout,
  kReadFailed,
  kFileStoreTimeout,
  kFileStoreFailed,
  kFileOpenFailed,
  kFileReadFailed,
  kFileCloseFailed,
};

inline const char* errorString(Error error) {
  switch (error) {
    case Error::kNone:
      return "none";
    case Error::kConfigurationRejected:
      return "configuration rejected";
    case Error::kCommandEchoTimeout:
      return "command echo timeout";
    case Error::kPromptTimeout:
      return "data prompt timeout";
    case Error::kCommandRejected:
      return "command rejected";
    case Error::kGetResultTimeout:
      return "GET result timeout";
    case Error::kGetFailed:
      return "GET failed";
    case Error::kInvalidContentLength:
      return "invalid content length";
    case Error::kBodyTimeout:
      return "body timeout";
    case Error::kBodySinkRejected:
      return "body sink rejected";
    case Error::kReadResultTimeout:
      return "READ result timeout";
    case Error::kReadFailed:
      return "READ failed";
    case Error::kFileStoreTimeout:
      return "file store timeout";
    case Error::kFileStoreFailed:
      return "file store failed";
    case Error::kFileOpenFailed:
      return "file open failed";
    case Error::kFileReadFailed:
      return "file read failed";
    case Error::kFileCloseFailed:
      return "file close failed";
  }
  return "unknown";
}

template <typename Module>
class Client {
 public:
  using BodySink = std::function<bool(const uint8_t*, size_t)>;

  explicit Client(Module& module) : module_{module}, last_error_{Error::kNone} {}

  bool configure(int context_id) {
    if (!execute("AT+QHTTPCFG=\"contextid\"," +
                 std::to_string(context_id)) ||
        !execute("AT+QHTTPCFG=\"requestheader\",0") ||
        !execute("AT+QHTTPCFG=\"responseheader\",0")) {
      last_error_ = Error::kConfigurationRejected;
      return false;
    }
    last_error_ = Error::kNone;
    return true;
  }

  bool beginGet(const char* url, Response* response) {
    if (url == nullptr || response == nullptr) {
      last_error_ = Error::kCommandRejected;
      return false;
    }
    const size_t url_length = strlen(url);
    if (url_length == 0 || url_length > 3000) {
      last_error_ = Error::kCommandRejected;
      return false;
    }

    const std::string url_command =
        "AT+QHTTPURL=" + std::to_string(url_length) + ",60";
    if (!module_.writeAndWaitCommand(url_command, kCommandEchoTimeoutMs)) {
      last_error_ = Error::kCommandEchoTimeout;
      return false;
    }
    if (!waitForLine("CONNECT", kPromptTimeoutMs)) {
      last_error_ = Error::kPromptTimeout;
      return false;
    }
    module_.writeBinary(url, url_length);
    if (!waitForFinalOk(kPromptTimeoutMs)) {
      last_error_ = Error::kCommandRejected;
      return false;
    }

    if (!module_.writeAndWaitCommand("AT+QHTTPGET=80",
                                     kCommandEchoTimeoutMs)) {
      last_error_ = Error::kCommandEchoTimeout;
      return false;
    }

    bool saw_ok = false;
    bool saw_get_result = false;
    const uint32_t started_at = millis();
    while (millis() - started_at < kOperationTimeoutMs) {
      const std::string line = module_.readResponse(kReadSliceMs);
      if (line.empty()) {
        continue;
      }
      if (line == "OK") {
        saw_ok = true;
        if (saw_get_result) {
          break;
        }
        continue;
      }
      if (isErrorLine(line)) {
        last_error_ = Error::kCommandRejected;
        return false;
      }
      if (parseGetResult(line, response)) {
        saw_get_result = true;
        if (saw_ok) {
          break;
        }
      }
    }
    if (!saw_ok || !saw_get_result) {
      last_error_ = Error::kGetResultTimeout;
      return false;
    }
    if (response->result != 0 || response->status_code < 100) {
      last_error_ = Error::kGetFailed;
      return false;
    }
    if (response->content_length < 0) {
      last_error_ = Error::kInvalidContentLength;
      return false;
    }
    last_error_ = Error::kNone;
    return true;
  }

  bool readBody(size_t body_size, const BodySink& sink) {
    if (!sink || body_size == 0) {
      last_error_ = Error::kInvalidContentLength;
      return false;
    }
    if (!module_.writeAndWaitCommand("AT+QHTTPREAD=80",
                                     kCommandEchoTimeoutMs)) {
      last_error_ = Error::kCommandEchoTimeout;
      return false;
    }
    if (!waitForLine("CONNECT", kOperationTimeoutMs)) {
      last_error_ = Error::kPromptTimeout;
      return false;
    }

    static uint8_t buffer[kReadChunkSize];
    size_t received = 0;
    while (received < body_size) {
      const size_t request =
          min(body_size - received, static_cast<size_t>(sizeof(buffer)));
      if (!module_.readBinary(buffer, request, kOperationTimeoutMs)) {
        last_error_ = Error::kBodyTimeout;
        return false;
      }
      if (!sink(buffer, request)) {
        last_error_ = Error::kBodySinkRejected;
        return false;
      }
      received += request;
    }

    bool saw_ok = false;
    bool saw_read_result = false;
    int read_result = -1;
    const uint32_t started_at = millis();
    while (millis() - started_at < kReadResultTimeoutMs) {
      const std::string line = module_.readResponse(kReadSliceMs);
      if (line.empty()) {
        continue;
      }
      if (line == "OK") {
        saw_ok = true;
        if (saw_read_result) {
          break;
        }
        continue;
      }
      if (isErrorLine(line)) {
        last_error_ = Error::kReadFailed;
        return false;
      }
      if (sscanf(line.c_str(), "+QHTTPREAD: %d", &read_result) == 1) {
        saw_read_result = true;
        if (saw_ok) {
          break;
        }
      }
    }
    if (!saw_ok || !saw_read_result) {
      last_error_ = Error::kReadResultTimeout;
      return false;
    }
    if (read_result != 0) {
      last_error_ = Error::kReadFailed;
      return false;
    }
    last_error_ = Error::kNone;
    return true;
  }

  bool readBodyViaFile(size_t body_size, const BodySink& sink) {
    if (!sink || body_size == 0) {
      last_error_ = Error::kInvalidContentLength;
      return false;
    }

    // A previous interrupted OTA may have left the scratch file behind.
    // File-not-found is harmless here, so deliberately ignore the result.
    module_.executeCommand("AT+QFDEL=\"wio_ota.bin\"", 1000);

    if (!module_.writeAndWaitCommand(
            "AT+QHTTPREADFILE=\"wio_ota.bin\",80",
            kCommandEchoTimeoutMs)) {
      last_error_ = Error::kCommandEchoTimeout;
      return false;
    }
    bool saw_ok = false;
    bool saw_store_result = false;
    int store_result = -1;
    const uint32_t store_started_at = millis();
    while (millis() - store_started_at < kOperationTimeoutMs) {
      const std::string line = module_.readResponse(kReadSliceMs);
      if (line.empty()) {
        continue;
      }
      if (line == "OK") {
        saw_ok = true;
        if (saw_store_result) {
          break;
        }
        continue;
      }
      if (isErrorLine(line)) {
        last_error_ = Error::kFileStoreFailed;
        return false;
      }
      if (sscanf(line.c_str(), "+QHTTPREADFILE: %d", &store_result) == 1) {
        saw_store_result = true;
        if (saw_ok) {
          break;
        }
      }
    }
    if (!saw_ok || !saw_store_result) {
      last_error_ = Error::kFileStoreTimeout;
      return false;
    }
    if (store_result != 0) {
      last_error_ = Error::kFileStoreFailed;
      return false;
    }
    Serial.println("[HTTP] UFS response stored");

    int file_handle = -1;
    const WioCellularResult open_result = module_.queryCommand(
        "AT+QFOPEN=\"wio_ota.bin\",2",
        [&file_handle](const std::string& line) {
          return sscanf(line.c_str(), "+QFOPEN: %d", &file_handle) == 1;
        },
        1000);
    if (open_result != WioCellularResult::Ok || file_handle < 0) {
      module_.executeCommand("AT+QFDEL=\"wio_ota.bin\"", 1000);
      last_error_ = Error::kFileOpenFailed;
      return false;
    }
    Serial.printf("[HTTP] UFS file opened handle=%d\n", file_handle);

    static uint8_t buffer[kReadChunkSize];
    size_t received = 0;
    while (received < body_size) {
      const size_t request =
          min(body_size - received, static_cast<size_t>(sizeof(buffer)));
      const std::string command = "AT+QFREAD=" + std::to_string(file_handle) +
                                  "," + std::to_string(request);
      if (received == 0) {
        Serial.printf("[HTTP] UFS first read request=%u\n",
                      static_cast<unsigned>(request));
      }
      if (!module_.writeAndWaitCommand(command, kCommandEchoTimeoutMs)) {
        cleanupFile(file_handle);
        last_error_ = Error::kFileReadFailed;
        return false;
      }
      if (received == 0) {
        Serial.println("[HTTP] UFS first read echo received");
      }

      int read_length = -1;
      const uint32_t prompt_started_at = millis();
      while (millis() - prompt_started_at < kPromptTimeoutMs) {
        const std::string line = module_.readResponse(kReadSliceMs);
        if (sscanf(line.c_str(), "CONNECT %d", &read_length) == 1) {
          break;
        }
        if (!line.empty() && isErrorLine(line)) {
          break;
        }
      }
      if (read_length <= 0 ||
          static_cast<size_t>(read_length) != request ||
          !module_.readBinary(buffer, request, kOperationTimeoutMs)) {
        cleanupFile(file_handle);
        last_error_ = Error::kFileReadFailed;
        return false;
      }
      if (received == 0) {
        Serial.printf("[HTTP] UFS first read payload=%d\n", read_length);
      }
      if (!sink(buffer, request) || !waitForFinalOk(kPromptTimeoutMs)) {
        cleanupFile(file_handle);
        last_error_ = Error::kFileReadFailed;
        return false;
      }
      received += request;
    }

    if (module_.executeCommand(
            "AT+QFCLOSE=" + std::to_string(file_handle), 1000) !=
        WioCellularResult::Ok) {
      module_.executeCommand("AT+QFDEL=\"wio_ota.bin\"", 1000);
      last_error_ = Error::kFileCloseFailed;
      return false;
    }
    module_.executeCommand("AT+QFDEL=\"wio_ota.bin\"", 1000);
    last_error_ = Error::kNone;
    return true;
  }

  Error lastError() const { return last_error_; }

 private:
  static constexpr uint32_t kCommandEchoTimeoutMs = 60000;
  static constexpr uint32_t kPromptTimeoutMs = 60000;
  static constexpr uint32_t kOperationTimeoutMs = 90000;
  static constexpr uint32_t kReadResultTimeoutMs = 10000;
  static constexpr uint32_t kReadSliceMs = 1000;
  static constexpr size_t kReadChunkSize = 512;

  bool execute(const std::string& command) {
    return module_.executeCommand(command, 1000) == WioCellularResult::Ok;
  }

  static bool isErrorLine(const std::string& line) {
    return line == "ERROR" || line.compare(0, 12, "+CME ERROR: ") == 0 ||
           line.compare(0, 12, "+CMS ERROR: ") == 0;
  }

  bool waitForLine(const char* expected, uint32_t timeout_ms) {
    const uint32_t started_at = millis();
    while (millis() - started_at < timeout_ms) {
      const std::string line = module_.readResponse(kReadSliceMs);
      if (line == expected) {
        return true;
      }
      if (!line.empty() && isErrorLine(line)) {
        return false;
      }
    }
    return false;
  }

  bool waitForFinalOk(uint32_t timeout_ms) {
    const uint32_t started_at = millis();
    while (millis() - started_at < timeout_ms) {
      const std::string line = module_.readResponse(kReadSliceMs);
      if (line == "OK") {
        return true;
      }
      if (!line.empty() && isErrorLine(line)) {
        return false;
      }
    }
    return false;
  }

  static bool parseGetResult(const std::string& line, Response* response) {
    int result = -1;
    int status_code = -1;
    int content_length = -1;
    const int fields = sscanf(line.c_str(), "+QHTTPGET: %d,%d,%d", &result,
                              &status_code, &content_length);
    if (fields < 1) {
      return false;
    }
    response->result = result;
    response->status_code = fields >= 2 ? status_code : -1;
    response->content_length = fields >= 3 ? content_length : -1;
    return true;
  }

  void cleanupFile(int file_handle) {
    if (file_handle >= 0) {
      module_.executeCommand("AT+QFCLOSE=" + std::to_string(file_handle),
                             1000);
    }
    module_.executeCommand("AT+QFDEL=\"wio_ota.bin\"", 1000);
  }

  Module& module_;
  Error last_error_;
};

}  // namespace wio_bg770a_http
