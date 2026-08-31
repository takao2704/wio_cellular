#pragma once

#include <Arduino.h>
#include <WioCellular.h>
#include <WioBg770aHttp.h>
#include <WioOtaManifest.h>
#include <WioOtaSecurity.h>
#include <WioOta.h>

#include <functional>

namespace wio_ota_agent {

struct Config {
  const char* target_hardware = nullptr;
  const char* manifest_host = nullptr;
  uint16_t manifest_port = 80;
  const char* manifest_path = nullptr;
  const char* allowed_firmware_host = nullptr;
  uint16_t allowed_firmware_port = 80;
  int pdp_context_id = 1;
  SecurityPolicy security;
};

enum class Decision {
  kReject,
  kNoUpdate,
  kDefer,
  kDownloadAndVerify,
  kApply,
};

enum class Result {
  kNoUpdate,
  kDeferred,
  kRejected,
  kVerified,
  kActivated,
  kFailed,
};

enum class Error {
  kNone,
  kInvalidConfiguration,
  kHttpConfigurationFailed,
  kManifestUrlTooLong,
  kManifestGetFailed,
  kManifestResponseRejected,
  kManifestReadFailed,
  kManifestJsonInvalid,
  kManifestFieldsInvalid,
  kFirmwareUrlInvalid,
  kFirmwareHostRejected,
  kManifestSecurityRejected,
  kFirmwareGetFailed,
  kFirmwareResponseRejected,
  kFirmwareReadFailed,
  kModemPowerOffFailed,
  kWriterFailed,
};

const char* errorString(Error error);
const char* resultString(Result result);

class Agent {
 public:
  using DecisionCallback = std::function<Decision(const Manifest&)>;
  using ProgressCallback = std::function<void(size_t received, size_t total)>;

  Agent(WioCellularModule& module, const Config& config,
        Stream* logger = nullptr);
  Agent(const Agent&) = delete;
  Agent& operator=(const Agent&) = delete;

  Result check(const DecisionCallback& decide,
               const ProgressCallback& on_progress = ProgressCallback{});

  Error lastError() const { return last_error_; }
  wio_bg770a_http::Error lastHttpError() const { return last_http_error_; }
  wio_ota::Error lastWriterError() const { return last_writer_error_; }
  SecurityError lastSecurityError() const { return last_security_error_; }
  const Manifest& lastManifest() const { return last_manifest_; }

 private:
  using HttpClient = wio_bg770a_http::Client<WioCellularModule>;

  bool configurationIsValid() const;
  bool fetchManifest(HttpClient& client, char* output, size_t capacity);
  bool downloadFirmware(HttpClient& client, const Manifest& manifest,
                        const ProgressCallback& on_progress);
  bool buildHttpUrl(const char* host, uint16_t port, const char* path,
                    char* output, size_t capacity) const;
  void log(const char* message) const;
  void logf(const char* format, ...) const;
  Result fail(Error error);
  Result failHttp(Error error, const HttpClient& client);
  Result failWriter(wio_ota::Error error);

  WioCellularModule& module_;
  Config config_;
  Stream* logger_;
  wio_ota::Writer writer_;
  Manifest last_manifest_;
  Error last_error_;
  wio_bg770a_http::Error last_http_error_;
  wio_ota::Error last_writer_error_;
  SecurityError last_security_error_;
};

}  // namespace wio_ota_agent
