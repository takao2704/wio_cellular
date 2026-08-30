#include "WioOtaAgent.h"

#include <ArduinoJson.h>

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <limits>

namespace wio_ota_agent {
namespace {

constexpr size_t kManifestCapacity = 1024;

bool copyString(const char* source, char* destination, size_t capacity) {
  if (source == nullptr || destination == nullptr || capacity == 0) {
    return false;
  }
  const size_t length = strlen(source);
  if (length >= capacity) {
    return false;
  }
  memcpy(destination, source, length + 1);
  return true;
}

}  // namespace

const char* errorString(Error error) {
  switch (error) {
    case Error::kNone:
      return "none";
    case Error::kInvalidConfiguration:
      return "invalid configuration";
    case Error::kHttpConfigurationFailed:
      return "HTTP configuration failed";
    case Error::kManifestUrlTooLong:
      return "manifest URL too long";
    case Error::kManifestGetFailed:
      return "manifest GET failed";
    case Error::kManifestResponseRejected:
      return "manifest response rejected";
    case Error::kManifestReadFailed:
      return "manifest read failed";
    case Error::kManifestJsonInvalid:
      return "manifest JSON invalid";
    case Error::kManifestFieldsInvalid:
      return "manifest fields invalid";
    case Error::kFirmwareUrlInvalid:
      return "firmware URL invalid";
    case Error::kFirmwareHostRejected:
      return "firmware host rejected";
    case Error::kFirmwareGetFailed:
      return "firmware GET failed";
    case Error::kFirmwareResponseRejected:
      return "firmware response rejected";
    case Error::kFirmwareReadFailed:
      return "firmware read failed";
    case Error::kModemPowerOffFailed:
      return "modem power-off failed";
    case Error::kWriterFailed:
      return "Bank 1 writer failed";
  }
  return "unknown";
}

const char* resultString(Result result) {
  switch (result) {
    case Result::kNoUpdate:
      return "no update";
    case Result::kDeferred:
      return "deferred";
    case Result::kRejected:
      return "rejected";
    case Result::kVerified:
      return "verified";
    case Result::kActivated:
      return "activated";
    case Result::kFailed:
      return "failed";
  }
  return "unknown";
}

Agent::Agent(WioCellularModule& module, const Config& config, Stream* logger)
    : module_{module},
      config_{config},
      logger_{logger},
      writer_{},
      last_manifest_{},
      last_error_{Error::kNone},
      last_http_error_{wio_bg770a_http::Error::kNone},
      last_writer_error_{wio_ota::Error::kNone} {}

bool Agent::configurationIsValid() const {
  return config_.target_hardware != nullptr &&
         config_.target_hardware[0] != '\0' &&
         config_.manifest_host != nullptr &&
         config_.manifest_host[0] != '\0' && config_.manifest_port != 0 &&
         config_.manifest_path != nullptr &&
         config_.manifest_path[0] == '/' &&
         config_.allowed_firmware_host != nullptr &&
         config_.allowed_firmware_host[0] != '\0' &&
         config_.allowed_firmware_port != 0 && config_.pdp_context_id > 0;
}

void Agent::log(const char* message) const {
  if (logger_ != nullptr) {
    logger_->println(message);
  }
}

void Agent::logf(const char* format, ...) const {
  if (logger_ == nullptr) {
    return;
  }
  char buffer[256] = {};
  va_list args;
  va_start(args, format);
  vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);
  logger_->println(buffer);
}

Result Agent::fail(Error error) {
  last_error_ = error;
  logf("[OTA] failed: %s", errorString(error));
  return Result::kFailed;
}

Result Agent::failHttp(Error error, const HttpClient& client) {
  last_http_error_ = client.lastError();
  last_error_ = error;
  logf("[HTTP] failed: %s (%s)", errorString(error),
       wio_bg770a_http::errorString(last_http_error_));
  return Result::kFailed;
}

Result Agent::failWriter(wio_ota::Error error) {
  last_writer_error_ = error;
  last_error_ = Error::kWriterFailed;
  logf("[OTA] writer failed: %s", wio_ota::errorString(error));
  return Result::kFailed;
}

bool Agent::buildHttpUrl(const char* host, uint16_t port, const char* path,
                         char* output, size_t capacity) const {
  if (host == nullptr || path == nullptr || output == nullptr || capacity == 0) {
    return false;
  }
  const int length =
      port == 80 ? snprintf(output, capacity, "http://%s%s", host, path)
                 : snprintf(output, capacity, "http://%s:%u%s", host, port,
                            path);
  return length > 0 && static_cast<size_t>(length) < capacity;
}

bool Agent::fetchManifest(HttpClient& client, char* output, size_t capacity) {
  char url[512] = {};
  if (!buildHttpUrl(config_.manifest_host, config_.manifest_port,
                    config_.manifest_path, url, sizeof(url))) {
    last_error_ = Error::kManifestUrlTooLong;
    return false;
  }
  log("[HTTP] GET metadata manifest");
  wio_bg770a_http::Response response;
  if (!client.beginGet(url, &response)) {
    last_http_error_ = client.lastError();
    last_error_ = Error::kManifestGetFailed;
    return false;
  }
  logf("[HTTP] manifest status=%d length=%d", response.status_code,
       response.content_length);
  if (response.status_code != 200 || response.content_length <= 0 ||
      static_cast<size_t>(response.content_length) >= capacity) {
    last_error_ = Error::kManifestResponseRejected;
    return false;
  }

  size_t received = 0;
  if (!client.readBody(
          static_cast<size_t>(response.content_length),
          [output, capacity, &received](const uint8_t* data, size_t size) {
            if (received + size >= capacity) {
              return false;
            }
            memcpy(output + received, data, size);
            received += size;
            return true;
          })) {
    last_http_error_ = client.lastError();
    last_error_ = Error::kManifestReadFailed;
    return false;
  }
  output[received] = '\0';
  return true;
}

bool Agent::decodeHex(const char* hex, uint8_t* output,
                      size_t output_size) const {
  if (hex == nullptr || output == nullptr || strlen(hex) != output_size * 2) {
    return false;
  }
  for (size_t i = 0; i < output_size; ++i) {
    unsigned int value = 0;
    if (sscanf(hex + i * 2, "%2x", &value) != 1) {
      return false;
    }
    output[i] = static_cast<uint8_t>(value);
  }
  return true;
}

bool Agent::parseHttpUrl(const char* url, FirmwareLocation* location) const {
  constexpr char kPrefix[] = "http://";
  if (url == nullptr || location == nullptr ||
      strncmp(url, kPrefix, sizeof(kPrefix) - 1) != 0) {
    return false;
  }
  const char* authority = url + sizeof(kPrefix) - 1;
  const char* path = strchr(authority, '/');
  if (path == nullptr || !copyString(path, location->path,
                                     sizeof(location->path))) {
    return false;
  }
  const char* colon = static_cast<const char*>(
      memchr(authority, ':', static_cast<size_t>(path - authority)));
  const char* host_end = colon == nullptr ? path : colon;
  const size_t host_length = static_cast<size_t>(host_end - authority);
  if (host_length == 0 || host_length >= sizeof(location->host)) {
    return false;
  }
  memcpy(location->host, authority, host_length);
  location->host[host_length] = '\0';

  if (colon != nullptr) {
    char* end = nullptr;
    const unsigned long parsed = strtoul(colon + 1, &end, 10);
    if (end != path || parsed == 0 || parsed > 65535) {
      return false;
    }
    location->port = static_cast<uint16_t>(parsed);
  }
  return true;
}

bool Agent::parseManifest(const char* json, Manifest* manifest) {
  if (json == nullptr || manifest == nullptr) {
    last_error_ = Error::kManifestJsonInvalid;
    return false;
  }
  JsonDocument document;
  const DeserializationError json_error = deserializeJson(document, json);
  if (json_error) {
    last_error_ = Error::kManifestJsonInvalid;
    return false;
  }

  const int format = document["format"] | -1;
  const char* hardware = document["hardware"];
  const long version = document["version"] | -1L;
  const long image_size = document["size"] | -1L;
  const char* crc_text = document["crc16"];
  const char* sha_text = document["sha256"];
  const char* url = document["url"];
  if (format != 1 || hardware == nullptr ||
      strcmp(hardware, config_.target_hardware) != 0 || version < 0 ||
      static_cast<unsigned long>(version) >
          std::numeric_limits<uint32_t>::max() ||
      image_size < 8 ||
      image_size > static_cast<long>(wio_ota::kMaximumImageSize) ||
      !copyString(hardware, manifest->hardware,
                  sizeof(manifest->hardware)) ||
      !copyString(url, manifest->url, sizeof(manifest->url))) {
    last_error_ = Error::kManifestFieldsInvalid;
    return false;
  }

  uint8_t crc_bytes[2] = {};
  if (!decodeHex(crc_text, crc_bytes, sizeof(crc_bytes)) ||
      !decodeHex(sha_text, manifest->sha256, sizeof(manifest->sha256))) {
    last_error_ = Error::kManifestFieldsInvalid;
    return false;
  }
  manifest->crc16 =
      (static_cast<uint16_t>(crc_bytes[0]) << 8) | crc_bytes[1];
  if (manifest->crc16 == 0) {
    last_error_ = Error::kManifestFieldsInvalid;
    return false;
  }

  FirmwareLocation location;
  if (!parseHttpUrl(manifest->url, &location)) {
    last_error_ = Error::kFirmwareUrlInvalid;
    return false;
  }
  if (strcmp(location.host, config_.allowed_firmware_host) != 0 ||
      location.port != config_.allowed_firmware_port) {
    last_error_ = Error::kFirmwareHostRejected;
    return false;
  }

  manifest->format = format;
  manifest->version = static_cast<uint32_t>(version);
  manifest->image_size = static_cast<size_t>(image_size);
  manifest->firmware_port = location.port;
  copyString(location.host, manifest->firmware_host,
             sizeof(manifest->firmware_host));
  copyString(location.path, manifest->firmware_path,
             sizeof(manifest->firmware_path));
  return true;
}

bool Agent::downloadFirmware(HttpClient& client, const Manifest& manifest,
                             const ProgressCallback& on_progress) {
  log("[HTTP] GET firmware image");
  wio_bg770a_http::Response response;
  if (!client.beginGet(manifest.url, &response)) {
    last_http_error_ = client.lastError();
    last_error_ = Error::kFirmwareGetFailed;
    return false;
  }
  logf("[HTTP] firmware status=%d length=%d", response.status_code,
       response.content_length);
  if (response.status_code != 200 || response.content_length < 0 ||
      static_cast<size_t>(response.content_length) != manifest.image_size) {
    last_error_ = Error::kFirmwareResponseRejected;
    return false;
  }

  if (const auto error = writer_.begin(manifest.image_size);
      error != wio_ota::Error::kNone) {
    last_writer_error_ = error;
    last_error_ = Error::kWriterFailed;
    return false;
  }

  if (!client.readBodyViaFile(
          manifest.image_size,
          [this, &on_progress](const uint8_t* data, size_t size) {
            if (const auto error = writer_.write(data, size);
                error != wio_ota::Error::kNone) {
              last_writer_error_ = error;
              last_error_ = Error::kWriterFailed;
              return false;
            }
            if (on_progress) {
              on_progress(writer_.bytesWritten(), writer_.imageSize());
            }
            return true;
          })) {
    if (last_error_ != Error::kWriterFailed) {
      last_http_error_ = client.lastError();
      last_error_ = Error::kFirmwareReadFailed;
    }
    writer_.discard();
    return false;
  }

  if (const auto error = writer_.finish(manifest.crc16, manifest.sha256);
      error != wio_ota::Error::kNone) {
    last_writer_error_ = error;
    last_error_ = Error::kWriterFailed;
    writer_.discard();
    return false;
  }
  log("[OTA] image verified");
  return true;
}

Result Agent::check(const DecisionCallback& decide,
                    const ProgressCallback& on_progress) {
  last_error_ = Error::kNone;
  last_http_error_ = wio_bg770a_http::Error::kNone;
  last_writer_error_ = wio_ota::Error::kNone;
  last_manifest_ = Manifest{};
  if (!configurationIsValid() || !decide) {
    return fail(Error::kInvalidConfiguration);
  }

  HttpClient client{module_};
  if (!client.configure(config_.pdp_context_id)) {
    return failHttp(Error::kHttpConfigurationFailed, client);
  }
  log("[HTTP] modem HTTP client configured");

  // OTA checks are serialized. Keep the receive buffer off the Arduino task
  // stack because the application also owns the parsed Manifest object.
  static char manifest_json[kManifestCapacity] = {};
  manifest_json[0] = '\0';
  if (!fetchManifest(client, manifest_json, sizeof(manifest_json))) {
    if (last_http_error_ != wio_bg770a_http::Error::kNone) {
      logf("[HTTP] manifest failed: %s (%s)", errorString(last_error_),
           wio_bg770a_http::errorString(last_http_error_));
    } else {
      logf("[OTA] manifest failed: %s", errorString(last_error_));
    }
    return Result::kFailed;
  }
  if (!parseManifest(manifest_json, &last_manifest_)) {
    return fail(last_error_);
  }

  const Decision decision = decide(last_manifest_);
  switch (decision) {
    case Decision::kReject:
      log("[OTA] update rejected by application");
      return Result::kRejected;
    case Decision::kNoUpdate:
      log("[OTA] application reports no update");
      return Result::kNoUpdate;
    case Decision::kDefer:
      log("[OTA] update deferred by application");
      return Result::kDeferred;
    case Decision::kDownloadAndVerify:
    case Decision::kApply:
      break;
  }

  logf("[OTA] downloading version=%lu size=%u",
       static_cast<unsigned long>(last_manifest_.version),
       static_cast<unsigned>(last_manifest_.image_size));
  if (!downloadFirmware(client, last_manifest_, on_progress)) {
    if (last_error_ == Error::kWriterFailed) {
      return failWriter(last_writer_error_);
    }
    if (last_http_error_ != wio_bg770a_http::Error::kNone) {
      return failHttp(last_error_, client);
    }
    return fail(last_error_);
  }

  if (decision == Decision::kDownloadAndVerify) {
    log("[OTA] verified only; application did not request activation");
    writer_.discard();
    return Result::kVerified;
  }

  if (module_.powerOff() != WioCellularResult::Ok) {
    writer_.discard();
    return fail(Error::kModemPowerOffFailed);
  }
  if (const auto error = writer_.activate();
      error != wio_ota::Error::kNone) {
    return failWriter(error);
  }
  log("[OTA] activated; rebooting");
  if (logger_ != nullptr) {
    logger_->flush();
  }
  delay(100);
  writer_.resetToApply();
  return Result::kActivated;
}

}  // namespace wio_ota_agent
