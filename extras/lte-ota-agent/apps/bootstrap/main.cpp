#include <Arduino.h>
#include <Adafruit_TinyUSB.h>
#include <WioOta.h>

namespace {

constexpr uint32_t kSerialBaudRate = 115200;
constexpr size_t kChunkSize = 512;
constexpr uint32_t kReceiveTimeoutMs = 30000;

wio_ota::Writer writer;
uint8_t buffer[kChunkSize];

bool readLine(char* output, size_t capacity, uint32_t timeout_ms) {
  size_t length = 0;
  const uint32_t started = millis();
  while (millis() - started < timeout_ms) {
    while (Serial.available()) {
      const int value = Serial.read();
      if (value == '\n') {
        output[length] = '\0';
        return true;
      }
      if (value != '\r' && length + 1 < capacity) {
        output[length++] = static_cast<char>(value);
      }
    }
    delay(1);
  }
  return false;
}

bool receiveExact(uint8_t* output, size_t size, uint32_t timeout_ms) {
  size_t received = 0;
  uint32_t last_byte_at = millis();
  while (received < size && millis() - last_byte_at < timeout_ms) {
    const int available = Serial.available();
    if (available <= 0) {
      delay(1);
      continue;
    }
    const size_t request =
        min(size - received, static_cast<size_t>(available));
    const size_t count = Serial.readBytes(output + received, request);
    if (count > 0) {
      received += count;
      last_byte_at = millis();
    }
  }
  return received == size;
}

void printError(const char* operation, wio_ota::Error error) {
  Serial.print("ERR ");
  Serial.print(operation);
  Serial.print(' ');
  Serial.println(wio_ota::errorString(error));
}

bool decodeSha256(const char* hex, uint8_t output[wio_ota::kSha256Size]) {
  if (strlen(hex) != wio_ota::kSha256Size * 2) {
    return false;
  }
  for (size_t i = 0; i < wio_ota::kSha256Size; ++i) {
    unsigned int value = 0;
    if (sscanf(hex + i * 2, "%2x", &value) != 1) {
      return false;
    }
    output[i] = static_cast<uint8_t>(value);
  }
  return true;
}

void handleUpdate(const char* command) {
  unsigned long image_size = 0;
  unsigned int expected_crc = 0;
  char expected_sha_hex[65] = {};
  uint8_t expected_sha[wio_ota::kSha256Size];
  if (sscanf(command, "WIOOTA %lu %x %64s", &image_size, &expected_crc,
             expected_sha_hex) != 3 ||
      expected_crc > 0xffffu ||
      !decodeSha256(expected_sha_hex, expected_sha)) {
    Serial.println(
        "ERR header expected: WIOOTA <size> <crc16-hex> <sha256-hex>");
    return;
  }

  if (const auto error = writer.begin(image_size);
      error != wio_ota::Error::kNone) {
    printError("begin", error);
    return;
  }

  Serial.print("READY ");
  Serial.println(image_size);

  while (writer.bytesWritten() < writer.imageSize()) {
    const size_t remaining = writer.imageSize() - writer.bytesWritten();
    const size_t request = min(remaining, sizeof(buffer));
    if (!receiveExact(buffer, request, kReceiveTimeoutMs)) {
      Serial.println("ERR receive timeout");
      return;
    }
    if (const auto error = writer.write(buffer, request);
        error != wio_ota::Error::kNone) {
      printError("write", error);
      return;
    }
    if ((writer.bytesWritten() % (16 * 1024)) == 0 ||
        writer.bytesWritten() == writer.imageSize()) {
      Serial.print("PROGRESS ");
      Serial.print(writer.bytesWritten());
      Serial.print('/');
      Serial.println(writer.imageSize());
    }
  }

  if (const auto error =
          writer.finish(static_cast<uint16_t>(expected_crc), expected_sha);
      error != wio_ota::Error::kNone) {
    printError("verify", error);
    return;
  }

  Serial.print("VERIFIED crc16=");
  Serial.println(writer.calculatedCrc(), HEX);
  Serial.println("SEND APPLY TO COMMIT, OR RESET TO ABORT");

  char apply_command[16];
  if (!readLine(apply_command, sizeof(apply_command), kReceiveTimeoutMs) ||
      strcmp(apply_command, "APPLY") != 0) {
    writer.discard();
    Serial.println("ABORTED not committed");
    return;
  }

  if (const auto error = writer.activate();
      error != wio_ota::Error::kNone) {
    printError("activate", error);
    return;
  }

  Serial.println("ACTIVATED rebooting");
  Serial.flush();
  delay(100);
  writer.resetToApply();
}

}  // namespace

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LED_STATE_ON);
  Serial.begin(kSerialBaudRate);
  const uint32_t wait_started = millis();
  while (!Serial && millis() - wait_started < 5000) {
    delay(10);
  }
  Serial.println("WIO OTA bootstrap v0.1 (HW 1.0, local transport)");
  Serial.println("WAITING WIOOTA <size> <crc16-hex> <sha256-hex>");
}

void loop() {
  // The protocol header contains a 64-character SHA-256 value and is
  // currently 82 characters for the largest supported image size.
  char command[96];
  if (readLine(command, sizeof(command), 1000)) {
    handleUpdate(command);
  }
  digitalToggle(LED_BUILTIN);
}
