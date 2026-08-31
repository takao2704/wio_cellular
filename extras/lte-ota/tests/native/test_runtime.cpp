#include "runtime_fakes.h"
#include "WioOtaAgent.h"
#include "WioOtaVersionStore.h"
#include "WioOtaCrc16.h"
#include <sstream>
#include <iomanip>

namespace {
constexpr const char* kA = "/wio-ota-version-a.bin";
constexpr const char* kB = "/wio-ota-version-b.bin";
using namespace wio_ota_agent;

SecurityError signature_result = SecurityError::kNone;
unsigned signature_calls = 0;

SecurityError verifySignature(const Manifest& manifest,
                              const uint8_t* public_key) {
  ++signature_calls;
  assert(manifest.format == 2 && manifest.has_signature);
  assert(public_key != nullptr);
  return signature_result;
}

void testVersionStore() {
  test_fs::reset();
  VersionStore fresh;
  assert(fresh.begin() && fresh.highestInstalledVersion() == 0);
  assert(fresh.recordCurrentVersion(5));
  assert(fresh.recordCurrentVersion(7));
  assert(fresh.recordCurrentVersion(6));
  assert(test_fs::writes == 2);
  const auto intact = test_fs::files;
  VersionStore reboot;
  assert(reboot.begin() && reboot.highestInstalledVersion() == 7);
  test_fs::files[kB][0] ^= 1;
  VersionStore one_bad;
  assert(one_bad.begin() && one_bad.highestInstalledVersion() == 5);
  assert(one_bad.recordCurrentVersion(8));
  assert(one_bad.highestInstalledVersion() == 8);

  test_fs::files = intact;
  test_fs::files[kA][0] ^= 1;
  test_fs::files[kB][0] ^= 1;
  const auto corrupt = test_fs::files;
  VersionStore both_bad;
  assert(!both_bad.begin()); // Original implementation incorrectly succeeds.
  assert(both_bad.lastError() == VersionStoreError::kRecordsInvalid);
  assert(!both_bad.recordCurrentVersion(9));
  assert(test_fs::files == corrupt);
  for (const char* path : {kA, kB}) {
    test_fs::reset(); test_fs::files[path] = corrupt.at(path);
    VersionStore bad_and_missing; assert(!bad_and_missing.begin());
    test_fs::reset(); test_fs::files[path] = intact.at(path);
    VersionStore good_and_missing;
    assert(good_and_missing.begin());
    assert(good_and_missing.highestInstalledVersion() == (path == kA ? 5u : 7u));
    assert(good_and_missing.recordCurrentVersion(10));
    assert(test_fs::files[path] == intact.at(path));
  }

  test_fs::reset();
  test_fs::files = intact;
  test_fs::mount_ok = false;
  VersionStore bad_mount;
  assert(!bad_mount.begin());
  assert(bad_mount.lastError() == VersionStoreError::kMountFailed);
  assert(test_fs::formats == 0 && test_fs::files == intact);
  assert(!bad_mount.recordCurrentVersion(9));
  assert(bad_mount.lastError() == VersionStoreError::kMountFailed);

  for (int error : {LFS_ERR_IO, LFS_ERR_CORRUPT}) {
    test_fs::reset();
    test_fs::stat_errors[kA] = error;
    VersionStore bad_stat;
    assert(!bad_stat.begin());
    assert(test_fs::writes == 0);
  }
  for (const char* path : {kA, kB}) {
    test_fs::reset(); test_fs::files = intact;
    test_fs::open_failure = path;
    VersionStore open_error;
    assert(open_error.begin()); // Other valid slot still recovers.
    test_fs::read_failure = path == kA ? kB : kA;
    assert(!open_error.begin());
    assert(!open_error.recordCurrentVersion(9));
    assert(test_fs::files == intact);
  }
  test_fs::reset(); test_fs::files[kA] = {1, 2};
  VersionStore truncated;
  assert(!truncated.begin());
  test_fs::reset(); test_fs::directory_path = kA;
  assert(!truncated.begin());
  test_fs::reset(); test_fs::files = intact; test_fs::files[kB].push_back(0);
  VersionStore extended;
  assert(extended.begin() && extended.highestInstalledVersion() == 5);
  for (int failure = 0; failure < 4; ++failure) {
    test_fs::reset(); test_fs::files = intact;
    VersionStore store; assert(store.begin());
    if (failure == 0) test_fs::remove_ok = false;
    if (failure == 1) test_fs::open_failure = kA;
    if (failure == 2) test_fs::short_write = true;
    if (failure == 3) test_fs::corrupt_write = true;
    assert(!store.recordCurrentVersion(9));
    assert(store.highestInstalledVersion() == 7);
    assert(test_fs::files[kB] == intact.at(kB));
    test_fs::remove_ok = true; test_fs::open_failure.clear();
    test_fs::short_write = test_fs::corrupt_write = false;
    assert(store.recordCurrentVersion(9)); // Retry still uses the damaged slot.
    assert(test_fs::files[kB] == intact.at(kB));
  }
  test_fs::reset();
  VersionStore not_started;
  assert(!not_started.recordCurrentVersion(0));
  assert(!not_started.recordCurrentVersion(9) && test_fs::writes == 0);
  assert(not_started.lastError() == VersionStoreError::kNotInitialized);
  assert(not_started.begin());
  assert(not_started.recordCurrentVersion(0) && test_fs::writes == 0);
  assert(not_started.recordCurrentVersion(UINT32_MAX));
  assert(not_started.recordCurrentVersion(UINT32_MAX - 1));
  assert(test_fs::writes == 1);
  VersionStore max_version;
  assert(max_version.begin() && max_version.highestInstalledVersion() == UINT32_MAX);
  test_fs::files[kB] = test_fs::files[kA]; // Equal versions prefer A, replace B.
  test_fs::files = intact;
  test_fs::files[kB] = intact.at(kA);
  VersionStore tied; assert(tied.begin());
  assert(tied.recordCurrentVersion(6));
  assert(test_fs::files[kA] == intact.at(kA));
  test_fs::mount_ok = false;
  assert(!tied.begin() && !tied.recordCurrentVersion(0));
  // Mirror the documented sample gate: an unavailable store never calls OTA.
  bool checked = false;
  bool ready = tied.begin();
  if (ready) ready = tied.recordCurrentVersion(10);
  if (ready) checked = true;
  assert(!checked);
  assert(test_fs::locks == 0 && test_fs::open_files == 0);
}

std::vector<uint8_t> image() {
  std::vector<uint8_t> bytes(1024, 0x35);
  const uint32_t vectors[] = {0x20001000, wio_ota::kApplicationAddress + 9};
  std::memcpy(bytes.data(), vectors, sizeof(vectors));
  return bytes;
}
std::string hexDigest(const std::vector<uint8_t>& bytes) {
  wio_ota::Sha256 sha; sha.update(bytes.data(), bytes.size());
  uint8_t digest[32]; sha.finish(digest);
  std::ostringstream result;
  for (auto byte : digest) result << std::hex << std::setw(2) << std::setfill('0') << unsigned(byte);
  return result.str();
}
void prepare(WioCellularModule& module) {
  module.firmware = image();
  char crc[5]; snprintf(crc, sizeof(crc), "%04x", wio_ota::crc16Ccitt(module.firmware.data(), module.firmware.size()));
  module.manifest = "{\"format\":1,\"hardware\":\"wio-bg770a-v1.0\",\"version\":10,"
    "\"url\":\"http://harvest-files.soracom.io/test.bin\",\"size\":1024,\"crc16\":\"" +
    std::string(crc) + "\",\"sha256\":\"" + hexDigest(module.firmware) + "\"}";
}

void prepareSigned(WioCellularModule& module) {
  prepare(module);
  char crc[5];
  snprintf(crc, sizeof(crc), "%04x",
           wio_ota::crc16Ccitt(module.firmware.data(), module.firmware.size()));
  module.manifest =
      "{\"format\":2,\"hardware\":\"wio-bg770a-v1.0\",\"version\":10,"
      "\"url\":\"http://harvest-files.soracom.io/test.bin\",\"size\":1024,"
      "\"crc16\":\"" + std::string(crc) + "\",\"sha256\":\"" +
      hexDigest(module.firmware) +
      "\",\"release_id\":\"release-10\",\"rollout\":10000,"
      "\"key_id\":\"test-key\",\"signature\":\"" +
      std::string(128, '0') + "\"}";
}

Config config() {
  Config value;
  value.target_hardware = "wio-bg770a-v1.0";
  value.manifest_host = "metadata.soracom.io";
  value.manifest_path = "/v1/userdata";
  value.allowed_firmware_host = "harvest-files.soracom.io";
  return value;
}

void testSignedAgentOrdering() {
  std::memset(test_flash::bytes, 0xff, sizeof(test_flash::bytes));
  auto* settings = static_cast<uint16_t*>(
      wioOtaTestFlashAddress(wio_ota::kBootloaderSettingsAddress));
  settings[0] = 1;
  settings[14] = 1;
  WioCellularModule module;
  prepareSigned(module);
  auto secure_config = config();
  uint8_t public_key[kManifestEd25519PublicKeySize] = {};
  secure_config.security.require_signature = true;
  secure_config.security.manifest_public_key = public_key;
  secure_config.security.expected_key_id = "test-key";
  setNativeSignatureVerifierForTest(verifySignature);

  signature_result = SecurityError::kNone;
  signature_calls = 0;
  unsigned decision_calls = 0;
  Agent agent(module, secure_config);
  assert(agent.check([&](const Manifest&) {
           ++decision_calls;
           assert(signature_calls == 1);
           return Decision::kNoUpdate;
         }) == Result::kNoUpdate);
  assert(signature_calls == 1 && decision_calls == 1);

  signature_result = SecurityError::kSignatureInvalid;
  assert(agent.check([&](const Manifest&) {
           ++decision_calls;
           return Decision::kApply;
         }) == Result::kRejected);
  assert(signature_calls == 2 && decision_calls == 1);
  assert(agent.lastSecurityError() == SecurityError::kSignatureInvalid);
  setNativeSignatureVerifierForTest(nullptr);
}

void testHttpFailuresAndRecovery() {
  WioCellularModule module;
  prepare(module);
  wio_bg770a_http::Client<WioCellularModule> http(module);
  wio_bg770a_http::Response response;
  const char* firmware_url =
      "http://harvest-files.soracom.io/test.bin";
  auto reset_http = [&]() {
    assert(http.modemResetRequired());
    assert(module.powerOff() == WioCellularResult::Ok);
    http.acknowledgeModemReset();
    assert(!http.modemResetRequired());
  };

  module.execute_failure = "AT+QHTTPCFG=\"requestheader\"";
  assert(!http.configure(1));
  assert(http.lastError() ==
         wio_bg770a_http::Error::kConfigurationFailed);
  assert(!http.modemResetRequired());
  module.execute_failure.clear();
  assert(http.configure(1));

  for (const auto timeout : {WioCellularResult::WaitCommandTimeout,
                             WioCellularResult::ReadResponseTimeout}) {
    module.execute_failure = "AT+QHTTPCFG=\"requestheader\"";
    module.execute_failure_result = timeout;
    assert(!http.configure(1));
    assert(http.lastError() ==
           wio_bg770a_http::Error::kConfigurationFailed);
    assert(module.delayed_command_reads > 0);
    reset_http();
  }
  module.execute_failure.clear();
  module.execute_failure_result = WioCellularResult::CommandRejected;
  assert(http.configure(1));

  assert(!http.beginGet(nullptr, &response));
  assert(http.lastError() == wio_bg770a_http::Error::kInvalidArgument);
  assert(!http.beginGet(firmware_url, nullptr));
  assert(http.lastError() == wio_bg770a_http::Error::kInvalidArgument);
  assert(!http.readBody(1, {}));
  assert(http.lastError() == wio_bg770a_http::Error::kInvalidArgument);
  assert(!http.modemResetRequired());
  assert(!http.readBodyViaFile(1, {}));
  assert(http.lastError() == wio_bg770a_http::Error::kInvalidArgument);
  assert(!http.modemResetRequired());
  module.echo_failure = "AT+QHTTPURL=";
  assert(!http.beginGet(firmware_url, &response));
  assert(http.lastError() == wio_bg770a_http::Error::kCommandEchoTimeout);
  module.echo_failure.clear();
  reset_http();
  module.reject_url_prompt = true;
  assert(!http.beginGet(firmware_url, &response));
  assert(http.lastError() == wio_bg770a_http::Error::kCommandRejected);
  assert(!http.modemResetRequired());
  module.reject_url_prompt = false;
  module.omit_url_prompt = true;
  assert(!http.beginGet(firmware_url, &response));
  assert(http.lastError() == wio_bg770a_http::Error::kPromptTimeout);
  module.omit_url_prompt = false;
  reset_http();
  module.reject_url = true;
  assert(!http.beginGet(firmware_url, &response));
  assert(http.lastError() == wio_bg770a_http::Error::kCommandRejected);
  assert(!http.modemResetRequired());
  module.reject_url = false;
  module.omit_url_result = true;
  assert(!http.beginGet(firmware_url, &response));
  assert(http.lastError() ==
         wio_bg770a_http::Error::kFinalResultTimeout);
  module.omit_url_result = false;
  reset_http();
  module.echo_failure = "AT+QHTTPGET=";
  assert(!http.beginGet(firmware_url, &response));
  assert(http.lastError() == wio_bg770a_http::Error::kCommandEchoTimeout);
  module.echo_failure.clear();
  reset_http();
  module.omit_get_result = true;
  assert(!http.beginGet(firmware_url, &response));
  assert(http.lastError() == wio_bg770a_http::Error::kGetResultTimeout);
  module.omit_get_result = false;
  assert(!http.beginGet(firmware_url, &response));
  assert(http.lastError() ==
         wio_bg770a_http::Error::kModemResetRequired);
  reset_http();
  module.delayed_get_result_once = true;
  assert(!http.beginGet(firmware_url, &response));
  assert(http.lastError() == wio_bg770a_http::Error::kGetResultTimeout);
  assert(module.delayed_get_reads > 0);
  assert(!http.beginGet(firmware_url, &response));
  assert(http.lastError() ==
         wio_bg770a_http::Error::kModemResetRequired);
  reset_http();
  module.get_failure = true;
  assert(!http.beginGet(firmware_url, &response));
  assert(http.lastError() == wio_bg770a_http::Error::kGetFailed);
  module.get_failure = false;
  module.invalid_content_length = true;
  assert(!http.beginGet(firmware_url, &response));
  assert(http.lastError() ==
         wio_bg770a_http::Error::kInvalidContentLength);
  module.invalid_content_length = false;
  assert(http.beginGet(firmware_url, &response));
  assert(response.modem_result_code == 0 && response.content_length == 1024);

  assert(!http.readBody(0, [](const uint8_t*, size_t) { return true; }));
  auto begin_firmware = [&]() {
    assert(http.beginGet(firmware_url, &response));
  };
  begin_firmware();
  module.echo_failure = "AT+QHTTPREAD=";
  assert(!http.readBody(1024, [](const uint8_t*, size_t) { return true; }));
  assert(http.lastError() == wio_bg770a_http::Error::kCommandEchoTimeout);
  module.echo_failure.clear();
  reset_http();
  begin_firmware();
  module.reject_read_prompt = true;
  assert(!http.readBody(1024, [](const uint8_t*, size_t) { return true; }));
  assert(http.lastError() == wio_bg770a_http::Error::kCommandRejected);
  assert(!http.modemResetRequired());
  module.reject_read_prompt = false;
  begin_firmware();
  module.omit_read_prompt = true;
  assert(!http.readBody(1024, [](const uint8_t*, size_t) { return true; }));
  assert(http.lastError() == wio_bg770a_http::Error::kPromptTimeout);
  module.omit_read_prompt = false;
  reset_http();
  begin_firmware();
  module.binary_failure = true;
  assert(!http.readBody(1024, [](const uint8_t*, size_t) { return true; }));
  assert(http.lastError() == wio_bg770a_http::Error::kBodyTimeout);
  module.binary_failure = false;
  reset_http();
  begin_firmware();
  assert(!http.readBody(1024,
                        [](const uint8_t*, size_t) { return false; }));
  assert(http.lastError() == wio_bg770a_http::Error::kBodySinkRejected);
  reset_http();
  begin_firmware();
  module.omit_read_result = true;
  assert(!http.readBody(1024, [](const uint8_t*, size_t) { return true; }));
  assert(http.lastError() == wio_bg770a_http::Error::kReadResultTimeout);
  module.omit_read_result = false;
  reset_http();
  begin_firmware();
  module.delayed_read_result_once = true;
  assert(!http.readBody(1024, [](const uint8_t*, size_t) { return true; }));
  assert(http.lastError() == wio_bg770a_http::Error::kReadResultTimeout);
  assert(module.delayed_read_reads > 0);
  assert(!http.beginGet(firmware_url, &response));
  assert(http.lastError() ==
         wio_bg770a_http::Error::kModemResetRequired);
  reset_http();
  begin_firmware();
  module.read_failure = true;
  assert(!http.readBody(1024, [](const uint8_t*, size_t) { return true; }));
  assert(http.lastError() == wio_bg770a_http::Error::kReadFailed);
  module.read_failure = false;
  begin_firmware();
  assert(http.readBody(1024,
                       [](const uint8_t*, size_t) { return true; }));

  begin_firmware();
  module.store_timeout_once = true;
  const unsigned deletes_before_timeout = module.deletes;
  assert(!http.readBodyViaFile(
      1024, [](const uint8_t*, size_t) { return true; }));
  assert(http.lastError() == wio_bg770a_http::Error::kFileStoreTimeout);
  assert(http.lastCleanupError() == wio_bg770a_http::Error::kNone);
  assert(module.delayed_store_reads == 0);
  assert(module.deletes >= deletes_before_timeout + 2);
  begin_firmware();
  assert(http.readBodyViaFile(
      1024, [](const uint8_t*, size_t) { return true; }));

  begin_firmware();
  module.short_file_read = true;
  assert(!http.readBodyViaFile(
      1024, [](const uint8_t*, size_t) { return true; }));
  assert(http.lastError() == wio_bg770a_http::Error::kFileReadFailed);
  assert(http.modemResetRequired());
  assert(!module.responses.empty());
  module.short_file_read = false;
  reset_http();

  begin_firmware();
  module.store_failure = true;
  assert(!http.readBodyViaFile(
      1024, [](const uint8_t*, size_t) { return true; }));
  assert(http.lastError() == wio_bg770a_http::Error::kFileStoreFailed);
  module.store_failure = false;
  begin_firmware();
  module.file_open_failure = true;
  assert(!http.readBodyViaFile(
      1024, [](const uint8_t*, size_t) { return true; }));
  assert(http.lastError() == wio_bg770a_http::Error::kFileOpenFailed);
  module.file_open_failure = false;
  begin_firmware();
  module.file_open_timeout_once = true;
  assert(!http.readBodyViaFile(
      1024, [](const uint8_t*, size_t) { return true; }));
  assert(http.lastError() == wio_bg770a_http::Error::kFileOpenFailed);
  assert(module.delayed_command_reads > 0);
  reset_http();
  begin_firmware();
  assert(http.readBodyViaFile(
      1024, [](const uint8_t*, size_t) { return true; }));

  begin_firmware();
  module.delete_failure_after = 2;
  assert(!http.readBodyViaFile(
      1024, [](const uint8_t*, size_t) { return true; }));
  assert(http.lastError() == wio_bg770a_http::Error::kFileCleanupFailed);
  assert(http.lastCleanupError() ==
         wio_bg770a_http::Error::kFileCleanupFailed);
  assert(!http.modemResetRequired());
  begin_firmware();
  assert(!http.readBodyViaFile(
      1024, [](const uint8_t*, size_t) { return false; }));
  assert(http.lastError() == wio_bg770a_http::Error::kFileReadFailed);
  assert(http.lastCleanupError() == wio_bg770a_http::Error::kNone);
  reset_http();
  begin_firmware();
  assert(http.readBodyViaFile(
      1024, [](const uint8_t*, size_t) { return true; }));

  begin_firmware();
  module.close_timeout_once = true;
  assert(!http.readBodyViaFile(
      1024, [](const uint8_t*, size_t) { return true; }));
  assert(http.lastError() == wio_bg770a_http::Error::kFileCloseFailed);
  assert(http.lastCleanupError() ==
         wio_bg770a_http::Error::kFileCloseFailed);
  assert(module.delayed_command_reads > 0);
  reset_http();

  begin_firmware();
  module.delete_timeout_after = 1;
  assert(!http.readBodyViaFile(
      1024, [](const uint8_t*, size_t) { return true; }));
  assert(http.lastError() == wio_bg770a_http::Error::kFileCleanupFailed);
  assert(http.lastCleanupError() ==
         wio_bg770a_http::Error::kFileCleanupFailed);
  assert(module.delayed_command_reads > 0);
  reset_http();

  begin_firmware();
  module.delete_timeout_after = 2;
  assert(!http.readBodyViaFile(
      1024, [](const uint8_t*, size_t) { return true; }));
  assert(http.lastError() == wio_bg770a_http::Error::kFileCleanupFailed);
  assert(http.lastCleanupError() ==
         wio_bg770a_http::Error::kFileCleanupFailed);
  assert(module.delayed_command_reads > 0);
  reset_http();

  Stream logger;
  wio_bg770a_http::Client<WioCellularModule> logged_http(module, &logger);
  assert(logged_http.beginGet(firmware_url, &response));
  assert(logged_http.readBodyViaFile(
      1024, [](const uint8_t*, size_t) { return true; }));
  assert(!logger.lines.empty());
  assert(Serial.lines.empty());
}
void testAgentAndWriter() {
  std::memset(test_flash::bytes, 0xff, sizeof(test_flash::bytes));
  auto* settings = static_cast<uint16_t*>(wioOtaTestFlashAddress(wio_ota::kBootloaderSettingsAddress));
  settings[0] = 1; settings[14] = 1; // Valid bank 0 and settings_version.
  std::vector<uint8_t> original_settings(reinterpret_cast<uint8_t*>(settings),
                                        reinterpret_cast<uint8_t*>(settings) + 4096);
  Config config;
  config.target_hardware = "wio-bg770a-v1.0";
  config.manifest_host = "metadata.soracom.io";
  config.manifest_path = "/v1/userdata";
  config.allowed_firmware_host = "harvest-files.soracom.io";
  WioCellularModule module; prepare(module);
  Agent agent(module, config);
  for (const auto pair : {std::make_pair(Decision::kReject, Result::kRejected),
                         std::make_pair(Decision::kNoUpdate, Result::kNoUpdate),
                         std::make_pair(Decision::kDefer, Result::kDeferred)}) {
    unsigned calls = 0;
    assert(agent.check([&](const Manifest& manifest) { ++calls; assert(manifest.version == 10); return pair.first; }) == pair.second);
    assert(calls == 1 && module.firmware_gets == 0);
    assert(test_flash::erases == 0 && test_flash::resets == 0 && module.power_offs == 0);
  }
  for (unsigned attempt = 1; attempt <= 2; ++attempt) {
    size_t last_progress = 0;
    assert(agent.check([](const Manifest&) { return Decision::kDownloadAndVerify; },
      [&](size_t received, size_t total) { assert(received > last_progress && total == 1024); last_progress = received; }) == Result::kVerified);
    assert(last_progress == 1024 && module.firmware_gets == attempt);
    assert(module.power_offs == 0 && test_flash::resets == 0);
    assert(std::memcmp(settings, original_settings.data(), 4096) == 0);
  }
  module.execute_failure = "AT+QHTTPCFG=\"requestheader\"";
  module.execute_failure_result = WioCellularResult::ReadResponseTimeout;
  const unsigned power_offs_before_config_timeout = module.power_offs;
  assert(agent.check([](const Manifest&) {
           return Decision::kNoUpdate;
         }) == Result::kFailed);
  assert(agent.lastError() == Error::kHttpConfigurationFailed);
  assert(agent.lastHttpRecoveryAttempted());
  assert(!agent.lastHttpRecoveryFailed());
  assert(module.power_offs == power_offs_before_config_timeout + 1);
  module.execute_failure.clear();
  module.execute_failure_result = WioCellularResult::CommandRejected;
  assert(agent.check([](const Manifest&) {
           return Decision::kNoUpdate;
         }) == Result::kNoUpdate);
  test_flash::softdevice = true;
  assert(agent.check([](const Manifest&) {
           return Decision::kDownloadAndVerify;
         }) == Result::kFailed);
  assert(agent.lastWriterError() == wio_ota::Error::kSoftDeviceEnabled);
  assert(agent.lastHttpRecoveryAttempted());
  assert(!agent.lastHttpRecoveryFailed());
  test_flash::softdevice = false;
  assert(agent.check([](const Manifest&) {
           return Decision::kDownloadAndVerify;
         }) == Result::kVerified);
  // Failed UFS transfer cleans up and the same Agent can retry successfully.
  for (int failure = 0; failure < 3; ++failure) {
    unsigned closed_before = module.closes;
    unsigned deleted_before = module.deletes;
    module.binary_failure = failure == 0;
    module.short_file_read = failure == 1;
    module.close_failure = failure == 2;
    assert(agent.check([](const Manifest&) { return Decision::kDownloadAndVerify; }) == Result::kFailed);
    assert(module.closes == closed_before + (failure == 2 ? 1u : 0u));
    assert(module.deletes >= deleted_before + (failure == 2 ? 2u : 1u));
    assert(agent.lastHttpRecoveryAttempted() == (failure < 2));
    assert(!agent.lastHttpRecoveryFailed());
    module.binary_failure = module.short_file_read = module.close_failure = false;
    assert(agent.check([](const Manifest&) { return Decision::kDownloadAndVerify; }) == Result::kVerified);
    assert(agent.lastError() == Error::kNone && agent.lastHttpError() == wio_bg770a_http::Error::kNone);
    assert(std::memcmp(settings, original_settings.data(), 4096) == 0);
  }
  module.binary_failure = true;
  module.power_off_failure = true;
  assert(agent.check([](const Manifest&) {
           return Decision::kDownloadAndVerify;
         }) == Result::kFailed);
  assert(agent.lastError() == Error::kModemPowerOffFailed);
  assert(agent.lastHttpRecoveryAttempted());
  assert(agent.lastHttpRecoveryFailed());
  module.binary_failure = false;
  module.power_off_failure = false;
  assert(agent.check([](const Manifest&) {
           return Decision::kDownloadAndVerify;
         }) == Result::kVerified);
  module.power_off_failure = true;
  assert(agent.check([](const Manifest&) { return Decision::kApply; }) == Result::kFailed);
  assert(test_flash::resets == 0);
  module.power_off_failure = false;
  assert(agent.check([](const Manifest&) { return Decision::kApply; }) == Result::kActivated);
  assert(test_flash::resets == 1 && settings[2] == 1);

  // A sink refusal skips unsafe AT cleanup; retry only after modem reset.
  wio_bg770a_http::Client<WioCellularModule> http(module);
  wio_bg770a_http::Response response;
  assert(http.beginGet("http://harvest-files.soracom.io/test.bin", &response));
  const unsigned before_close = module.closes;
  assert(!http.readBodyViaFile(1024, [](const uint8_t*, size_t) { return false; }));
  assert(module.closes == before_close);
  assert(http.modemResetRequired());
  assert(module.powerOff() == WioCellularResult::Ok);
  http.acknowledgeModemReset();
  assert(http.beginGet("http://harvest-files.soracom.io/test.bin", &response));
  assert(http.readBodyViaFile(1024, [](const uint8_t*, size_t) { return true; }));

  wio_ota::Writer writer;
  auto bytes = image();
  test_flash::softdevice = true;
  assert(writer.begin(bytes.size()) == wio_ota::Error::kSoftDeviceEnabled);
  test_flash::softdevice = false; test_flash::erase_fails = true;
  assert(writer.begin(bytes.size()) == wio_ota::Error::kFlashEraseFailed);
  test_flash::erase_fails = false;
  assert(writer.begin(bytes.size()) == wio_ota::Error::kNone);
  test_flash::write_fails = true;
  assert(writer.write(bytes.data(), bytes.size()) == wio_ota::Error::kFlashVerifyFailed);
  test_flash::write_fails = false;
  assert(writer.begin(bytes.size()) == wio_ota::Error::kNone);
  assert(writer.write(bytes.data(), bytes.size()) == wio_ota::Error::kNone);
  writer.discard();
  assert(writer.bytesWritten() == 0 && !writer.isVerified() && !writer.isActivated());
  assert(std::memcmp(wioOtaTestFlashAddress(wio_ota::kBank1Address), bytes.data(), bytes.size()) == 0);
  assert(writer.activate() == wio_ota::Error::kInvalidState);
  assert(writer.begin(bytes.size()) == wio_ota::Error::kNone);
  assert(writer.write(bytes.data(), bytes.size()) == wio_ota::Error::kNone);
  uint8_t digest[32]; wio_ota::Sha256 sha;
  sha.update(bytes.data(), bytes.size()); sha.finish(digest);
  const uint16_t crc = wio_ota::crc16Ccitt(bytes.data(), bytes.size());
  assert(writer.finish(crc, digest) == wio_ota::Error::kNone);
  writer.discard();
  assert(writer.activate() == wio_ota::Error::kInvalidState);

  auto stage = [&](const std::vector<uint8_t>& candidate) {
    writer.discard();
    assert(writer.begin(candidate.size()) == wio_ota::Error::kNone);
    assert(writer.write(candidate.data(), candidate.size()) ==
           wio_ota::Error::kNone);
    uint8_t candidate_digest[32];
    wio_ota::Sha256 candidate_sha;
    candidate_sha.update(candidate.data(), candidate.size());
    candidate_sha.finish(candidate_digest);
    return writer.finish(
        wio_ota::crc16Ccitt(candidate.data(), candidate.size()),
        candidate_digest);
  };
  auto expect_invalid_vectors = [&](uint32_t stack_pointer,
                                    uint32_t reset_handler) {
    auto invalid = image();
    const uint32_t vectors[] = {stack_pointer, reset_handler};
    std::memcpy(invalid.data(), vectors, sizeof(vectors));
    assert(stage(invalid) == wio_ota::Error::kInvalidVectorTable);
    assert(writer.activate() == wio_ota::Error::kInvalidState);
  };
  expect_invalid_vectors(0x20001001, wio_ota::kApplicationAddress + 9);
  expect_invalid_vectors(0x1ffffffc, wio_ota::kApplicationAddress + 9);
  expect_invalid_vectors(0x20040004, wio_ota::kApplicationAddress + 9);
  expect_invalid_vectors(0x20001000, wio_ota::kApplicationAddress + 8);
  expect_invalid_vectors(0x20001000, wio_ota::kApplicationAddress - 3);
  expect_invalid_vectors(0x20001000,
                         wio_ota::kApplicationAddress + bytes.size() + 1);

  settings[2] = 0;
  settings[14] = 2;
  assert(stage(bytes) == wio_ota::Error::kNone);
  assert(writer.activate() ==
         wio_ota::Error::kIncompatibleBootloaderSettings);
  assert(settings[2] == 0);
  settings[14] = 1;
  settings[0] = 0;
  assert(stage(bytes) == wio_ota::Error::kNone);
  assert(writer.activate() ==
         wio_ota::Error::kIncompatibleBootloaderSettings);
  assert(settings[2] == 0);
  settings[0] = 1;

  const unsigned resets_before_activation_failures = test_flash::resets;
  assert(stage(bytes) == wio_ota::Error::kNone);
  test_flash::erase_fails = true;
  assert(writer.activate() == wio_ota::Error::kFlashEraseFailed);
  assert(test_flash::resets == resets_before_activation_failures);
  test_flash::erase_fails = false;
  settings[0] = 1;
  settings[2] = 0;
  settings[14] = 1;
  assert(stage(bytes) == wio_ota::Error::kNone);
  test_flash::write_fails = true;
  assert(writer.activate() == wio_ota::Error::kFlashVerifyFailed);
  assert(test_flash::resets == resets_before_activation_failures);
  test_flash::write_fails = false;
  std::memset(settings, 0xff, 4096);
  settings[0] = 1;
  settings[2] = 0;
  settings[14] = 1;
}

template <typename Enum, typename Function>
void expectAllPublicStrings(Function function, int last_value) {
  for (int value = 0; value <= last_value; ++value) {
    assert(std::strcmp(function(static_cast<Enum>(value)), "unknown") != 0);
  }
  assert(std::strcmp(function(static_cast<Enum>(last_value + 1)), "unknown") ==
         0);
}

void testPublicStringMappings() {
  expectAllPublicStrings<Error>(errorString,
                                static_cast<int>(Error::kWriterFailed));
  expectAllPublicStrings<Result>(resultString,
                                 static_cast<int>(Result::kFailed));
  expectAllPublicStrings<wio_bg770a_http::Error>(
      wio_bg770a_http::errorString,
      static_cast<int>(wio_bg770a_http::Error::kModemResetRequired));
  expectAllPublicStrings<wio_ota::Error>(
      wio_ota::errorString,
      static_cast<int>(wio_ota::Error::kIncompatibleBootloaderSettings));
  expectAllPublicStrings<ManifestError>(
      manifestErrorString,
      static_cast<int>(ManifestError::kFirmwareHostRejected));
  expectAllPublicStrings<SecurityError>(
      securityErrorString,
      static_cast<int>(SecurityError::kRolloutNotSelected));
  expectAllPublicStrings<wio_ota::VerificationError>(
      wio_ota::verificationErrorString,
      static_cast<int>(wio_ota::VerificationError::kCrcDisabledValue));
  expectAllPublicStrings<VersionStoreError>(
      versionStoreErrorString,
      static_cast<int>(VersionStoreError::kNotInitialized));
}
}
int main() {
  testVersionStore();
  testSignedAgentOrdering();
  testHttpFailuresAndRecovery();
  testAgentAndWriter();
  testPublicStringMappings();
  puts("runtime state and recovery tests: PASS");
}
