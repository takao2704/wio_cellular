/*
 * soracom-connectivity-pubsubclient.ino
 * Copyright (C) Seeed K.K.
 * MIT License
 */

////////////////////////////////////////////////////////////////////////////////
// Libraries:
//   http://librarymanager#PubSubClient 2.8

#include <Adafruit_TinyUSB.h>
#include <WioCellular.h>
#include <PubSubClient.h>

#define SEARCH_ACCESS_TECHNOLOGY (WioCellularNetwork::SearchAccessTechnology::LTEM)
#define LTEM_BAND (WioCellularNetwork::NTTDOCOMO_LTEM_BAND)
static const char APN[] = "soracom.io";

static constexpr int POWER_ON_TIMEOUT = 1000 * 20;  // [ms]

#define ABORT_IF_FAILED(result) \
  do { \
    if ((result) != WioCellularResult::Ok) abort(); \
  } while (0)

static constexpr int PDP_CONTEXT_ID = 1;
static constexpr int SOCKET_ID = 0;
static WioCellularTcpClient<WioCellularModule> TcpClient{ WioCellular, PDP_CONTEXT_ID, SOCKET_ID };

#define ENDPOINT "beam.soracom.io"
#define PORT (1883)
#define SUBSCRIBE_TOPIC "test/cmd"

PubSubClient mqttClient(TcpClient);

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);                // Display topic
  Serial.println("] ");

  String payload_str = "";
  for (int i = 0; i < length; i++) {  // Parse Message
    payload_str += (char)payload[i];
  }
  Serial.println(payload_str);
}

void connectToMQTTS() {
  Serial.print("Connecting to MQTTS broker via SORACOM Beam(Proxy): ");
  mqttClient.setServer(ENDPOINT, PORT);
  mqttClient.setCallback(callback);
  mqttClient.connect(SUBSCRIBE_TOPIC);
  mqttClient.subscribe(SUBSCRIBE_TOPIC, 0);
  Serial.println("Connected!");
}

void setup(void) {
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

  WioCellular.begin();
  ABORT_IF_FAILED(WioCellular.powerOn(POWER_ON_TIMEOUT));

  WioNetwork.config.searchAccessTechnology = SEARCH_ACCESS_TECHNOLOGY;
  WioNetwork.config.ltemBand = LTEM_BAND;
  WioNetwork.config.apn = APN;
  WioNetwork.begin();

  connectToMQTTS();

  digitalWrite(LED_BUILTIN, LOW);
}

void loop(void) {
  if (!mqttClient.connected()) {
    if (mqttClient.connect(SUBSCRIBE_TOPIC)) {
      Serial.println("Connected");
      mqttClient.subscribe(SUBSCRIBE_TOPIC, 0);
      Serial.println("Subscribed");
    }
  }
  mqttClient.loop();
}
