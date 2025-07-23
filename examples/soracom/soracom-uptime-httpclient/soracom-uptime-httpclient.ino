/*
 * soracom-uptime-httpclient.ino
 * Copyright (C) Seeed K.K.
 * MIT License
 */

////////////////////////////////////////////////////////////////////////////////
// Libraries:
//   http://librarymanager#ArduinoJson 7.0.4
//   http://librarymanager#ArduinoHttpClient 0.6.1

#include <Adafruit_TinyUSB.h>
#include <csignal>
#include <map>
#include <WioCellular.h>
#include <ArduinoJson.h>
#include <ArduinoHttpClient.h>

#define SEARCH_ACCESS_TECHNOLOGY (WioCellularNetwork::SearchAccessTechnology::LTEM)  // https://seeedjp.github.io/Wiki/Wio_BG770A/kb/kb4.html
#define LTEM_BAND (WioCellularNetwork::NTTDOCOMO_LTEM_BAND)                          // https://seeedjp.github.io/Wiki/Wio_BG770A/kb/kb4.html
static const char APN[] = "soracom.io";

static const char HOST[] = "metadata.soracom.io";
static const char PATH[] = "/v1/subscriber/tags";
static constexpr int PORT = 80;

static constexpr int INTERVAL = 1000 * 60 * 5;         // [ms]
static constexpr int POWER_ON_TIMEOUT = 1000 * 20;     // [ms]
static constexpr int NETWORK_TIMEOUT = 1000 * 60 * 3;  // [ms]
static constexpr int RECEIVE_TIMEOUT = 1000 * 10;      // [ms]

static void abortHandler(int sig) {
  Serial.printf("ABORT: Signal %d received\n", sig);
  yield();

  vTaskSuspendAll();  // FreeRTOS
  while (true) {
    ledOn(LED_BUILTIN);
    nrfx_coredep_delay_us(100000);  // Spin
    ledOff(LED_BUILTIN);
    nrfx_coredep_delay_us(100000);  // Spin
  }
}

static JsonDocument JsonDoc;

void setup() {
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

void loop() {
  digitalWrite(LED_BUILTIN, HIGH);

  JsonDoc.clear();
  if (measure(JsonDoc)) {
    send(JsonDoc);
  }

  digitalWrite(LED_BUILTIN, LOW);

  WioCellular.doWorkUntil(INTERVAL);
}

/**
 * Generate request body for update SIM tag
 * See: https://users.soracom.io/ja-jp/docs/air/use-metadata/#%E3%83%A1%E3%82%BF%E3%83%87%E3%83%BC%E3%82%BF%E3%81%AE%E6%9B%B8%E3%81%8D%E8%BE%BC%E3%81%BF%E4%BE%8B-iot-sim-%E3%81%AE%E3%82%BF%E3%82%B0%E3%82%92%E8%BF%BD%E5%8A%A0--%E6%9B%B4%E6%96%B0%E3%81%99%E3%82%8B
 */
static bool measure(JsonDocument &doc) {
  Serial.println("### Measuring");

  JsonArray rootArray = doc.to<JsonArray>();

  JsonObject uptimeTag = rootArray.add<JsonObject>();
  uptimeTag["tagName"] = "UPTIME";
  uptimeTag["tagValue"] = std::to_string(millis() / 1000);

  Serial.println("### Completed");

  return true;
}

static bool send(const JsonDocument &doc) {
  Serial.print("### Requesting to [");
  Serial.print(HOST);
  Serial.println("]");

  int statusCode = -1;
  int contentLength = -1;
  String responseBody;
  {
    WioCellularArduinoTcpClient<WioCellularModule> client{ WioCellular, WioNetwork.config.pdpContextId };
    HttpClient httpClient{ client, HOST, PORT };
    httpClient.setTimeout(RECEIVE_TIMEOUT);

    std::string requestBody;
    serializeJson(doc, requestBody);
    Serial.println(requestBody.c_str());
    if (const auto result = httpClient.put(PATH, "application/json", requestBody.c_str()); result != 0) {
      Serial.printf("ERROR: Failed to HTTP PUT %d\n", result);
      return false;
    }

    statusCode = httpClient.responseStatusCode();
    if (statusCode >= 0) {
      contentLength = httpClient.contentLength();
      responseBody = httpClient.responseBody();
    }
  }

  Serial.println("### End HTTP request");
  Serial.println();

  Serial.print("Status code returned ");
  Serial.println(statusCode);
  Serial.print("Content length: ");
  Serial.println(contentLength);
  Serial.print("Body: ");
  Serial.println(responseBody);

  if (statusCode == 200) {
    JsonDocument doc;
    deserializeJson(doc, responseBody);
    // Output the IMSI field as an example of how to use the response
    Serial.print("Response imsi> ");
    Serial.print(doc["imsi"].as<String>());
    Serial.println();
  }

  return true;
}
