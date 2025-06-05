/*
 * cellular-mqtt-pubsubclient.ino
 * Copyright (C) Seeed K.K.
 * MIT License
 */

////////////////////////////////////////////////////////////////////////////////
// Libraries:
//   http://librarymanager#PubSubClient 2.8.0
//   http://librarymanager#ArduinoJson 7.0.4

#include <Adafruit_TinyUSB.h>
#include <csignal>
#include <WioCellular.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

#define SEARCH_ACCESS_TECHNOLOGY (WioCellularNetwork::SearchAccessTechnology::LTEM)  // https://seeedjp.github.io/Wiki/Wio_BG770A/kb/kb4.html
#define LTEM_BAND (WioCellularNetwork::NTTDOCOMO_LTEM_BAND)                          // https://seeedjp.github.io/Wiki/Wio_BG770A/kb/kb4.html
static const char APN[] = "soracom.io";

static const char MQTT_BROKER_HOST[] = "test.mosquitto.org";
static constexpr int MQTT_BROKER_PORT = 1883;
static constexpr int MQTT_KEEP_ALIVE = 60;

static constexpr int INTERVAL = 1000 * 60 * 5;      // [ms]
static constexpr int POWER_ON_TIMEOUT = 1000 * 20;  // [ms]
static constexpr int RECONNECT_WAIT_TIME = 1000;    // [ms]

static void abortHandler(int sig) {
  while (true) {
    ledOn(LED_BUILTIN);
    delay(100);
    ledOff(LED_BUILTIN);
    delay(100);
  }
}

static constexpr int PDP_CONTEXT_ID = 1;

static JsonDocument JsonDoc;
static WioCellularArduinoTcpClient<WioCellularModule> TcpClient{ WioCellular, PDP_CONTEXT_ID };
static PubSubClient MqttClient{ TcpClient };
static std::string ClientId;
static std::string SubscribeTopic;
static std::string PublishTopic;
static int ReconnectWaitTime = RECONNECT_WAIT_TIME;

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
  WioNetwork.config.pdpContextId = PDP_CONTEXT_ID;
  WioNetwork.config.apn = APN;

  // Start WioCellular
  WioCellular.begin();

  // Power on the cellular module
  if (WioCellular.powerOn(POWER_ON_TIMEOUT) != WioCellularResult::Ok) abort();
  WioNetwork.begin();

  std::string imsi;
  if (WioCellular.getIMSI(&imsi) != WioCellularResult::Ok) abort();
  ClientId = imsi;
  SubscribeTopic = std::string{ "WioCellular/" } + ClientId + "/downstream";
  PublishTopic = std::string{ "WioCellular/" } + ClientId + "/upstream";
  Serial.println();
  Serial.println("Commands:");
  Serial.printf(" mosquitto_sub -h %s -p %d -t \"%s\"\n", MQTT_BROKER_HOST, MQTT_BROKER_PORT, PublishTopic.c_str());
  Serial.printf(" mosquitto_pub -h %s -p %d -t \"%s\" -m \"message\"\n", MQTT_BROKER_HOST, MQTT_BROKER_PORT, SubscribeTopic.c_str());
  Serial.println();

  MqttClient.setServer(MQTT_BROKER_HOST, MQTT_BROKER_PORT);
  MqttClient.setKeepAlive(MQTT_KEEP_ALIVE);
  MqttClient.setCallback(MqttClientCallback);

  digitalWrite(LED_BUILTIN, LOW);
}

void loop() {
  if (!MqttClient.connected()) {
    Serial.print("Connecting ");
    Serial.print(MQTT_BROKER_HOST);
    Serial.print(":");
    Serial.println(MQTT_BROKER_PORT);
    if (!MqttClient.connect(ClientId.c_str())) {
      Serial.println("ERROR: Failed to connect");
      WioCellular.doWorkUntil(ReconnectWaitTime);

      ReconnectWaitTime *= 2;
    } else {
      ReconnectWaitTime = RECONNECT_WAIT_TIME;

      Serial.print("Subscribe ");
      Serial.println(SubscribeTopic.c_str());
      MqttClient.subscribe(SubscribeTopic.c_str());
    }
  } else {
    digitalWrite(LED_BUILTIN, HIGH);

    JsonDoc.clear();
    if (measure(JsonDoc)) {
      send(JsonDoc);
    }

    digitalWrite(LED_BUILTIN, LOW);

    const auto start = millis();
    while (millis() - start < static_cast<uint32_t>(INTERVAL)) {
      WioCellular.doWork(10);  // Spin

      MqttClient.loop();
      if (!MqttClient.connected()) break;
    }
  }
}

static bool measure(JsonDocument& doc) {
  Serial.println("### Measuring");

  doc["uptime"] = millis() / 1000;

  Serial.println("### Completed");

  return true;
}

static bool send(const JsonDocument& doc) {
  Serial.println("### Sending");

  Serial.println("Publish");
  std::string str;
  serializeJson(JsonDoc, str);
  Serial.printf(" Topic:   %s\n", PublishTopic.c_str());
  Serial.printf(" Message: %s\n", str.c_str());
  if (!MqttClient.publish(PublishTopic.c_str(), str.c_str())) {
    Serial.println("ERROR: Failed to publish");
    return false;
  }

  Serial.println("### Completed");

  return true;
}

static void MqttClientCallback(char* topic, byte* payload, unsigned int length) {
  char payloadStr[length + 1];
  memcpy(payloadStr, payload, length);
  payloadStr[length] = '\0';

  Serial.println("Message arrived");
  Serial.printf(" Topic:   %s\n", topic);
  Serial.printf(" Message: %s\n", payloadStr);
}
