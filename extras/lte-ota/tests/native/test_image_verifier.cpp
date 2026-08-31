#include <cassert>
#include <cstdint>
#include <cstring>

#include "WioOtaCrc16.h"
#include "WioOtaImageVerifier.h"
#include "WioOtaSha256.h"

namespace {

using wio_ota::ImageVerifier;
using wio_ota::VerificationError;

constexpr uint8_t kImage[] = {
    0x00, 0x10, 0x00, 0x20, 0x01, 0x70, 0x02, 0x00,
    0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70, 0x80,
};

void expectedDigest(uint8_t output[wio_ota::kSha256Size]) {
  wio_ota::Sha256 sha;
  sha.update(kImage, sizeof(kImage));
  sha.finish(output);
}

}  // namespace

int main() {
  const uint16_t expected_crc =
      wio_ota::crc16Ccitt(kImage, sizeof(kImage));
  uint8_t expected_sha[wio_ota::kSha256Size];
  expectedDigest(expected_sha);

  ImageVerifier complete;
  assert(complete.begin(sizeof(kImage)) == VerificationError::kNone);
  assert(complete.update(kImage, 5) == VerificationError::kNone);
  assert(complete.update(kImage + 5, sizeof(kImage) - 5) ==
         VerificationError::kNone);
  assert(complete.finish(expected_crc, expected_sha) ==
         VerificationError::kNone);
  assert(complete.isVerified());

  ImageVerifier interrupted;
  assert(interrupted.begin(sizeof(kImage)) == VerificationError::kNone);
  assert(interrupted.update(kImage, sizeof(kImage) / 2) ==
         VerificationError::kNone);
  assert(interrupted.finish(expected_crc, expected_sha) ==
         VerificationError::kImageIncomplete);
  assert(!interrupted.isVerified());

  ImageVerifier crc_mismatch;
  assert(crc_mismatch.begin(sizeof(kImage)) == VerificationError::kNone);
  assert(crc_mismatch.update(kImage, sizeof(kImage)) ==
         VerificationError::kNone);
  assert(crc_mismatch.finish(static_cast<uint16_t>(expected_crc ^ 1u),
                             expected_sha) ==
         VerificationError::kCrcMismatch);

  ImageVerifier sha_mismatch;
  uint8_t wrong_sha[wio_ota::kSha256Size];
  std::memcpy(wrong_sha, expected_sha, sizeof(wrong_sha));
  wrong_sha[0] ^= 1u;
  assert(sha_mismatch.begin(sizeof(kImage)) == VerificationError::kNone);
  assert(sha_mismatch.update(kImage, sizeof(kImage)) ==
         VerificationError::kNone);
  assert(sha_mismatch.finish(expected_crc, wrong_sha) ==
         VerificationError::kSha256Mismatch);

  ImageVerifier overflow;
  assert(overflow.begin(sizeof(kImage)) == VerificationError::kNone);
  assert(overflow.update(kImage, sizeof(kImage) + 1) ==
         VerificationError::kInvalidArgument);

  return 0;
}
