#!/usr/bin/env bash
set -euo pipefail

project_root="$(cd "$(dirname "$0")/.." && pwd)"
build_dir="$project_root/.pio/native-tests"
compiler="${CXX:-c++}"

arduinojson_header="$(
  rg --files "$project_root/.pio/libdeps" 2>/dev/null |
    rg '/ArduinoJson/src/ArduinoJson[.]h$' |
    head -n 1 || true
)"
if [[ -z "$arduinojson_header" ]]; then
  echo "ArduinoJson is not installed. Run: pio run -e lte_bootstrap" >&2
  exit 2
fi
arduinojson_include="$(dirname "$arduinojson_header")"

mkdir -p "$build_dir"

"$compiler" -std=c++17 -Wall -Wextra -Werror \
  -I"$project_root/lib/WioOta/src" \
  "$project_root/tests/native/test_crc16.cpp" \
  "$project_root/lib/WioOta/src/WioOtaCrc16.cpp" \
  -o "$build_dir/test_crc16"

"$compiler" -std=c++17 -Wall -Wextra -Werror \
  -I"$project_root/lib/WioOta/src" \
  "$project_root/tests/native/test_sha256.cpp" \
  "$project_root/lib/WioOta/src/WioOtaSha256.cpp" \
  -o "$build_dir/test_sha256"

"$compiler" -std=c++17 -Wall -Wextra -Werror \
  -I"$project_root/lib/WioOta/src" \
  "$project_root/tests/native/test_image_verifier.cpp" \
  "$project_root/lib/WioOta/src/WioOtaImageVerifier.cpp" \
  "$project_root/lib/WioOta/src/WioOtaCrc16.cpp" \
  "$project_root/lib/WioOta/src/WioOtaSha256.cpp" \
  -o "$build_dir/test_image_verifier"

"$compiler" -std=c++17 -Wall -Wextra -Werror \
  -I"$project_root/lib/WioOtaAgent/src" \
  -I"$arduinojson_include" \
  "$project_root/tests/native/test_manifest.cpp" \
  "$project_root/lib/WioOtaAgent/src/WioOtaManifest.cpp" \
  -o "$build_dir/test_manifest"

"$compiler" -std=c++17 -Wall -Wextra -Werror \
  -I"$project_root/lib/WioOta/src" \
  -I"$project_root/lib/WioOtaAgent/src" \
  "$project_root/tests/native/test_security.cpp" \
  "$project_root/lib/WioOtaAgent/src/WioOtaManifest.cpp" \
  "$project_root/lib/WioOtaAgent/src/WioOtaSecurity.cpp" \
  "$project_root/lib/WioOta/src/WioOtaSha256.cpp" \
  -I"$arduinojson_include" \
  -o "$build_dir/test_security"

"$build_dir/test_crc16"
"$build_dir/test_sha256"
"$build_dir/test_image_verifier"
"$build_dir/test_manifest"
"$build_dir/test_security"
python3 "$project_root/tests/python/test_manifest_signing.py"

echo "native tests: PASS"
