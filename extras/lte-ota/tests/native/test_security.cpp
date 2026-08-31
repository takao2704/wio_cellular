#include <cassert>
#include <cstdint>
#include <cstring>

#include "WioOtaSecurity.h"

namespace {

#if defined(WIO_OTA_NATIVE_TEST)
wio_ota_agent::SecurityError verifier_result =
    wio_ota_agent::SecurityError::kNone;
unsigned verifier_calls = 0;

wio_ota_agent::SecurityError verifySignature(
    const wio_ota_agent::Manifest& manifest, const uint8_t* public_key) {
  ++verifier_calls;
  assert(manifest.format == 2 && manifest.has_signature);
  assert(public_key != nullptr);
  return verifier_result;
}
#endif

wio_ota_agent::Manifest signedManifest() {
  wio_ota_agent::Manifest manifest;
  manifest.format = 2;
  std::strcpy(manifest.hardware, "wio-bg770a-v1.0");
  manifest.version = 7;
  manifest.image_size = 1024;
  manifest.crc16 = 0x1234;
  std::strcpy(manifest.url,
              "http://harvest-files.soracom.io/release-7/firmware.bin");
  std::strcpy(manifest.release_id, "release-7");
  manifest.rollout_basis_points = 10000;
  std::strcpy(manifest.key_id, "test-key");
  manifest.has_signature = true;
  return manifest;
}

}  // namespace

int main() {
  using wio_ota_agent::SecurityError;
  using wio_ota_agent::SecurityPolicy;

  auto manifest = signedManifest();
  SecurityPolicy policy;
  assert(wio_ota_agent::evaluateManifestSecurity(manifest, policy) ==
         SecurityError::kNone);

  policy.enforce_anti_rollback = true;
  policy.current_version = 6;
  assert(wio_ota_agent::evaluateManifestSecurity(manifest, policy) ==
         SecurityError::kNone);
  policy.highest_installed_version = 7;
  assert(wio_ota_agent::evaluateManifestSecurity(manifest, policy) ==
         SecurityError::kAlreadyInstalled);
  policy.highest_installed_version = 8;
  assert(wio_ota_agent::evaluateManifestSecurity(manifest, policy) ==
         SecurityError::kRollbackRejected);

  policy = SecurityPolicy{};
  policy.enforce_rollout = true;
  policy.rollout_device_id = "device-001";
  uint16_t bucket = 0;
  assert(wio_ota_agent::manifestRolloutBucket(
      policy.rollout_device_id, manifest.release_id, &bucket));
  assert(bucket < 10000);
  manifest.rollout_basis_points = bucket;
  assert(wio_ota_agent::evaluateManifestSecurity(manifest, policy) ==
         SecurityError::kRolloutNotSelected);
  manifest.rollout_basis_points = static_cast<uint16_t>(bucket + 1);
  assert(wio_ota_agent::evaluateManifestSecurity(manifest, policy) ==
         SecurityError::kNone);
  manifest.format = 1;
  manifest.has_signature = false;
  manifest.rollout_basis_points = 10000;
  assert(wio_ota_agent::evaluateManifestSecurity(manifest, policy) ==
         SecurityError::kRolloutRequired);
  manifest = signedManifest();

  policy = SecurityPolicy{};
  policy.require_signature = true;
  uint8_t public_key[wio_ota_agent::kManifestEd25519PublicKeySize] = {};
  policy.manifest_public_key = public_key;
  policy.expected_key_id = "other-key";
  assert(wio_ota_agent::evaluateManifestSecurity(manifest, policy) ==
         SecurityError::kKeyIdRejected);
  policy.expected_key_id = "test-key";
#if defined(WIO_OTA_NATIVE_TEST)
  wio_ota_agent::setNativeSignatureVerifierForTest(verifySignature);
  verifier_result = SecurityError::kNone;
  verifier_calls = 0;
  assert(wio_ota_agent::evaluateManifestSecurity(manifest, policy) ==
         SecurityError::kNone);
  assert(verifier_calls == 1);
  verifier_result = SecurityError::kSignatureInvalid;
  assert(wio_ota_agent::evaluateManifestSecurity(manifest, policy) ==
         SecurityError::kSignatureInvalid);
  assert(verifier_calls == 2);
  wio_ota_agent::setNativeSignatureVerifierForTest(nullptr);
#endif
  assert(wio_ota_agent::evaluateManifestSecurity(manifest, policy) ==
         SecurityError::kVerifierUnavailable);
  manifest.format = 1;
  manifest.has_signature = false;
  assert(wio_ota_agent::evaluateManifestSecurity(manifest, policy) ==
         SecurityError::kSignatureRequired);
  return 0;
}
