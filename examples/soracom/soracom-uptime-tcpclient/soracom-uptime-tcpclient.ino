/*
 * soracom-uptime-tcpclient.ino
 * Copyright (C) Seeed K.K.
 * MIT License
 */

////////////////////////////////////////////////////////////////////////////////
// Libraries:
//   http://librarymanager#ArduinoJson 7.0.4

#include <Adafruit_TinyUSB.h>
#include <csignal>
#include <WioCellular.h>
#include <ArduinoJson.h>

#define SEARCH_ACCESS_TECHNOLOGY (WioCellularNetwork::SearchAccessTechnology::LTEM)
#define LTEM_BAND (WioCellularNetwork::NTTDOCOMO_LTEM_BAND)
static const char APN[] = "soracom.io";

static const char HOST[] = "uni.soracom.io";
static constexpr int PORT = 23080;

static constexpr int INTERVAL = 1000 * 60 * 5;         // [ms]
static constexpr int POWER_ON_TIMEOUT = 1000 * 20;     // [ms]
static constexpr int NETWORK_TIMEOUT = 1000 * 60 * 2;  // [ms]
static constexpr int RECEIVE_TIMEOUT = 1000 * 10;      // [ms]

static void abortHandler(int sig) {
  while (true) {
    ledOn(LED_BUILTIN);
    delay(100);
    ledOff(LED_BUILTIN);
    delay(100);
  }
}

static JsonDocument JsonDoc;

void setup(void) {
  signal(SIGABRT, abortHandler);
  Serial.begin(115200);
  {
    const auto start = millis();
    while (!Serial && millis() - start < 5000) {
      delay(2);
    }
  }
  Serial.println();
  Serial.println();

  Serial.println("Startup");
  digitalWrite(LED_BUILTIN, HIGH);

  // Network configuration
  WioNetwork.config.searchAccessTechnology = SEARCH_ACCESS_TECHNOLOGY;
  WioNetwork.config.ltemBand = LTEM_BAND;
  WioNetwork.config.apn = APN;

  // Start WioCellular
  WioCellular.begin();

  // Power on the cellular module
  if (WioCellular.powerOn(POWER_ON_TIMEOUT) != WioCellularResult::Ok) abort();
  WioNetwork.begin();

  // Wait for communication available
  if (!WioNetwork.waitUntilCommunicationAvailable(NETWORK_TIMEOUT)) abort();

  digitalWrite(LED_BUILTIN, LOW);
}

void loop(void) {
  digitalWrite(LED_BUILTIN, HIGH);

  JsonDoc.clear();
  if (measure(JsonDoc)) {
    send(JsonDoc);
  }

  digitalWrite(LED_BUILTIN, LOW);

  WioCellular.doWorkUntil(INTERVAL);
}

static bool measure(JsonDocument& doc) {
  Serial.println("### Measuring");

  doc["uptime"] = millis() / 1000;

  Serial.println("### Completed");

  return true;
}

static bool send(const JsonDocument& doc) {
  Serial.println("### Sending");

  Serial.print("Connecting ");
  Serial.print(HOST);
  Serial.print(":");
  Serial.println(PORT);

  {
    WioCellularArduinoTcpClient<WioCellularModule> client{ WioCellular, WioNetwork.config.pdpContextId };
    if (!client.connect(HOST, PORT)) {
      Serial.println("ERROR: Failed to open socket");
      return false;
    }

    Serial.print("Sending ");
    std::string str;
    serializeJson(doc, str);
    printData(Serial, str.data(), str.size());
    Serial.println();
    if (client.write(reinterpret_cast<const uint8_t*>(str.data()), str.size()) != str.size()) {
      Serial.println("ERROR: Failed to send socket");
      return false;
    }

    Serial.println("Receiving");
    int availableSize;
    const auto start = millis();
    while ((availableSize = client.available()) == 0 && millis() - start < RECEIVE_TIMEOUT) {
      WioCellular.doWork(2);  // Spin
    }
    if (availableSize <= 0) {
      Serial.println("ERROR: Failed to available socket");
      return false;
    }

    static uint8_t recvData[WioCellular.RECEIVE_SOCKET_SIZE_MAX];
    const int recvSize = client.read(recvData, sizeof(recvData));
    if (recvSize <= 0) {
      Serial.println("ERROR: Failed to receive socket");
      return false;
    }

    printData(Serial, recvData, recvSize);
    Serial.println();

    client.stop();
  }

  Serial.println("### Completed");

  return true;
}

template<typename T>
void printData(T& stream, const void* data, size_t size) {
  auto p = static_cast<const char*>(data);

  for (; size > 0; --size, ++p)
    stream.write(0x20 <= *p && *p <= 0x7f ? *p : '.');
}
