#include <cassert>
#include <cstring>
#include <string>

#include "WioOtaManifest.h"

namespace {

constexpr size_t kMaximumImageSize = 397312;
constexpr char kValidSha256[] =
    "25968e40476c60548cdc9ddab54c905ccf946005d3b286c778b9cd4fff2b2b74";

wio_ota_agent::ManifestPolicy policy() {
  wio_ota_agent::ManifestPolicy value;
  value.target_hardware = "wio-bg770a-v1.0";
  value.allowed_firmware_host = "harvest-files.soracom.io";
  value.allowed_firmware_port = 80;
  value.maximum_image_size = kMaximumImageSize;
  return value;
}

std::string manifest(const char* hardware = "wio-bg770a-v1.0",
                     long long size = 129084,
                     const char* crc16 = "2f4f",
                     const char* sha256 = kValidSha256,
                     const char* url =
                         "http://harvest-files.soracom.io/firmware.bin") {
  return "{\"format\":1,\"hardware\":\"" + std::string(hardware) +
         "\",\"version\":2,\"url\":\"" + std::string(url) +
         "\",\"size\":" + std::to_string(size) +
         ",\"crc16\":\"" + std::string(crc16) +
         "\",\"sha256\":\"" + std::string(sha256) + "\"}";
}

std::string signedManifest(const char* signature = nullptr,
                           long long rollout = 2500) {
  const std::string signature_text =
      signature == nullptr ? std::string(128, '0') : std::string(signature);
  return "{\"format\":2,\"hardware\":\"wio-bg770a-v1.0\","
         "\"version\":2,\"url\":\"http://harvest-files.soracom.io/"
         "firmware.bin\",\"size\":129084,\"crc16\":\"2f4f\","
         "\"sha256\":\"" + std::string(kValidSha256) +
         "\",\"release_id\":\"release-2\",\"rollout\":" +
         std::to_string(rollout) +
         ",\"key_id\":\"test-2026\",\"signature\":\"" +
         signature_text + "\"}";
}

int nibble(char value) {
  return value <= '9' ? value - '0' : value - 'a' + 10;
}

std::string decodeHex(const char* hex) {
  std::string output;
  for (size_t index = 0; hex[index] != '\0'; index += 2) {
    output.push_back(static_cast<char>((nibble(hex[index]) << 4) |
                                       nibble(hex[index + 1])));
  }
  return output;
}

void expectError(const std::string& json,
                 wio_ota_agent::ManifestError expected,
                 const wio_ota_agent::ManifestPolicy& manifest_policy =
                     policy()) {
  wio_ota_agent::Manifest parsed;
  assert(wio_ota_agent::parseManifest(json.c_str(), manifest_policy, &parsed) ==
         expected);
}

}  // namespace

int main() {
  wio_ota_agent::Manifest parsed;
  assert(wio_ota_agent::parseManifest(manifest().c_str(), policy(), &parsed) ==
         wio_ota_agent::ManifestError::kNone);
  assert(parsed.version == 2);
  assert(parsed.image_size == 129084);
  assert(parsed.crc16 == 0x2f4f);
  assert(std::strcmp(parsed.firmware_host, "harvest-files.soracom.io") == 0);
  assert(std::strcmp(parsed.firmware_path, "/firmware.bin") == 0);
  assert(parsed.firmware_port == 80);

  assert(wio_ota_agent::parseManifest(signedManifest().c_str(), policy(),
                                      &parsed) ==
         wio_ota_agent::ManifestError::kNone);
  assert(parsed.has_signature);
  assert(std::strcmp(parsed.release_id, "release-2") == 0);
  assert(parsed.rollout_basis_points == 2500);
  assert(std::strcmp(parsed.key_id, "test-2026") == 0);
  uint8_t canonical[wio_ota_agent::kManifestCanonicalCapacity] = {};
  size_t canonical_size = 0;
  assert(wio_ota_agent::encodeManifestCanonical(
      parsed, canonical, sizeof(canonical), &canonical_size));
  constexpr char kExpectedCanonical[] =
      "57494f2d4f54412d4d414e49464553542d563200000002000f77696f2d6267"
      "373730612d76312e3000000002002c687474703a2f2f686172766573742d6669"
      "6c65732e736f7261636f6d2e696f2f6669726d776172652e62696e0001f83c"
      "2f4f25968e40476c60548cdc9ddab54c905ccf946005d3b286c778b9cd4fff"
      "2b2b74000972656c656173652d3209c40009746573742d32303236";
  const std::string expected_canonical = decodeHex(kExpectedCanonical);
  assert(canonical_size == expected_canonical.size());
  assert(std::memcmp(canonical, expected_canonical.data(), canonical_size) ==
         0);
  assert(!wio_ota_agent::encodeManifestCanonical(
      parsed, canonical, canonical_size - 1, &canonical_size));

  expectError(signedManifest("bad-signature"),
              wio_ota_agent::ManifestError::kFieldsInvalid);
  expectError(signedManifest(nullptr, 10001),
              wio_ota_agent::ManifestError::kFieldsInvalid);

  expectError(manifest("other-hardware"),
              wio_ota_agent::ManifestError::kFieldsInvalid);
  expectError(manifest("wio-bg770a-v1.0", 7),
              wio_ota_agent::ManifestError::kFieldsInvalid);
  expectError(manifest("wio-bg770a-v1.0", kMaximumImageSize + 1),
              wio_ota_agent::ManifestError::kFieldsInvalid);
  expectError(manifest("wio-bg770a-v1.0", 129084, "0000"),
              wio_ota_agent::ManifestError::kFieldsInvalid);
  expectError(manifest("wio-bg770a-v1.0", 129084, "2g4f"),
              wio_ota_agent::ManifestError::kFieldsInvalid);
  expectError(manifest("wio-bg770a-v1.0", 129084, "2f4f", "not-a-sha256"),
              wio_ota_agent::ManifestError::kFieldsInvalid);
  expectError(manifest("wio-bg770a-v1.0", 129084, "2f4f", kValidSha256,
                       "https://harvest-files.soracom.io/firmware.bin"),
              wio_ota_agent::ManifestError::kFirmwareUrlInvalid);
  expectError(manifest("wio-bg770a-v1.0", 129084, "2f4f", kValidSha256,
                       "http://example.com/firmware.bin"),
              wio_ota_agent::ManifestError::kFirmwareHostRejected);
  expectError(manifest("wio-bg770a-v1.0", 129084, "2f4f", kValidSha256,
                       "http://harvest-files.soracom.io:8080/firmware.bin"),
              wio_ota_agent::ManifestError::kFirmwareHostRejected);
  expectError(manifest("wio-bg770a-v1.0", 129084, "2f4f", kValidSha256,
                       "http://harvest-files.soracom.io"),
              wio_ota_agent::ManifestError::kFirmwareUrlInvalid);
  expectError("{not-json", wio_ota_agent::ManifestError::kJsonInvalid);

  auto custom_port_policy = policy();
  custom_port_policy.allowed_firmware_port = 8080;
  const auto custom_port_manifest =
      manifest("wio-bg770a-v1.0", 129084, "2f4f", kValidSha256,
               "http://harvest-files.soracom.io:8080/firmware.bin");
  assert(wio_ota_agent::parseManifest(custom_port_manifest.c_str(),
                                      custom_port_policy, &parsed) ==
         wio_ota_agent::ManifestError::kNone);
  assert(parsed.firmware_port == 8080);

  auto invalid_policy = policy();
  invalid_policy.maximum_image_size = 0;
  expectError(manifest(), wio_ota_agent::ManifestError::kInvalidPolicy,
              invalid_policy);
  return 0;
}
