#pragma once

#include <stddef.h>
#include <stdint.h>

#include "WioOtaManifest.h"

namespace wio_ota_agent {

struct SecurityPolicy {
  bool require_signature = false;
  const uint8_t* manifest_public_key = nullptr;
  const char* expected_key_id = nullptr;

  bool enforce_anti_rollback = false;
  uint32_t current_version = 0;
  uint32_t highest_installed_version = 0;

  bool enforce_rollout = false;
  const char* rollout_device_id = nullptr;
};

enum class SecurityError {
  kNone,
  kInvalidPolicy,
  kSignatureRequired,
  kKeyIdRejected,
  kCanonicalEncodingFailed,
  kVerifierUnavailable,
  kSignatureInvalid,
  kAlreadyInstalled,
  kRollbackRejected,
  kRolloutNotSelected,
};

const char* securityErrorString(SecurityError error);

// Returns a stable bucket in the range [0, 9999]. The raw device identifier
// is never placed in the manifest or log; only its SHA-256-derived bucket is
// used for rollout selection.
bool manifestRolloutBucket(const char* device_id, const char* release_id,
                           uint16_t* bucket);

SecurityError evaluateManifestSecurity(const Manifest& manifest,
                                       const SecurityPolicy& policy);

}  // namespace wio_ota_agent
