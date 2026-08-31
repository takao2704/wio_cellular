#!/usr/bin/env bash
set -euo pipefail

project_root="$(cd "$(dirname "$0")/.." && pwd)"
build_dir="$project_root/.pio/native-tests"
compiler="${CXX:-c++}"
python="${PYTHON:-python3}"

# Resolve the test dependency independently of any hardware diagnostic app.
if [[ -n "${ARDUINOJSON_INCLUDE_DIR:-}" ]]; then
  arduinojson_include="$ARDUINOJSON_INCLUDE_DIR"
else
  arduinojson_header="$(
    rg --files "$project_root/examples/CellularStatusOta/.pio/libdeps" 2>/dev/null |
      rg '/ArduinoJson/src/ArduinoJson[.]h$' |
      head -n 1 || true
  )"
  arduinojson_include="${arduinojson_header%/*}"
fi
if [[ ! -f "$arduinojson_include/ArduinoJson.h" ]]; then
  echo "ArduinoJson is not installed. Build the example first:" >&2
  echo "  pio run -d examples/CellularStatusOta -e initial" >&2
  echo "Or set ARDUINOJSON_INCLUDE_DIR to the ArduinoJson src directory." >&2
  exit 2
fi

mkdir -p "$build_dir"

# Runtime tests compile real OTA sources against host-only Core boundaries.
# Generate include-name aliases without shipping fake Core headers as libraries.
mkdir -p "$build_dir/runtime-includes"
for header in Arduino.h WioCellular.h nrfx_nvmc.h nrf_sdm.h Adafruit_LittleFS.h InternalFileSystem.h; do
  ln -sf "$project_root/tests/native/runtime_fakes.h" "$build_dir/runtime-includes/$header"
done
"$compiler" -std=c++17 -Wall -Wextra -Werror -DWIO_OTA_NATIVE_TEST \
  -I"$build_dir/runtime-includes" -I"$project_root/src" -I"$arduinojson_include" \
  "$project_root/tests/native/test_runtime.cpp" \
  "$project_root/src/WioOtaAgent.cpp" "$project_root/src/WioOta.cpp" \
  "$project_root/src/WioOtaVersionStore.cpp" "$project_root/src/WioOtaManifest.cpp" \
  "$project_root/src/WioOtaSecurity.cpp" "$project_root/src/WioOtaImageVerifier.cpp" \
  "$project_root/src/WioOtaCrc16.cpp" "$project_root/src/WioOtaSha256.cpp" \
  -o "$build_dir/test_runtime"
"$build_dir/test_runtime"

"$compiler" -std=c++17 -Wall -Wextra -Werror \
  -I"$project_root/src" \
  "$project_root/tests/native/test_crc16.cpp" \
  "$project_root/src/WioOtaCrc16.cpp" \
  -o "$build_dir/test_crc16"

"$compiler" -std=c++17 -Wall -Wextra -Werror \
  -I"$project_root/src" \
  "$project_root/tests/native/test_sha256.cpp" \
  "$project_root/src/WioOtaSha256.cpp" \
  -o "$build_dir/test_sha256"

"$compiler" -std=c++17 -Wall -Wextra -Werror \
  -I"$project_root/src" \
  "$project_root/tests/native/test_image_verifier.cpp" \
  "$project_root/src/WioOtaImageVerifier.cpp" \
  "$project_root/src/WioOtaCrc16.cpp" \
  "$project_root/src/WioOtaSha256.cpp" \
  -o "$build_dir/test_image_verifier"

"$compiler" -std=c++17 -Wall -Wextra -Werror \
  -I"$project_root/src" \
  -I"$arduinojson_include" \
  "$project_root/tests/native/test_manifest.cpp" \
  "$project_root/src/WioOtaManifest.cpp" \
  -o "$build_dir/test_manifest"

"$compiler" -std=c++17 -Wall -Wextra -Werror -DWIO_OTA_NATIVE_TEST \
  -I"$project_root/src" \
  "$project_root/tests/native/test_security.cpp" \
  "$project_root/src/WioOtaManifest.cpp" \
  "$project_root/src/WioOtaSecurity.cpp" \
  "$project_root/src/WioOtaSha256.cpp" \
  -I"$arduinojson_include" \
  -o "$build_dir/test_security"

"$build_dir/test_crc16"
"$build_dir/test_sha256"
"$build_dir/test_image_verifier"
"$build_dir/test_manifest"
"$build_dir/test_security"
"$python" "$project_root/tests/python/test_manifest_signing.py"
"$python" "$project_root/tests/python/test_arduino_support.py"

"$python" "$project_root/tests/python/test_project_layout.py"

echo "native tests: PASS"
