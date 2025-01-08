/*
 * soracom-uptime-lp.ino
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

static constexpr int START_DELAY = 1000 * 10;          // [ms]
static constexpr int MEASURE_PERIOD = 1000 * 60 * 5;   // [ms]
static constexpr int PSM_PERIOD = 60 * 6;              // [s]
static constexpr int PSM_ACTIVE = 2;                   // [s]
static constexpr int POWER_ON_TIMEOUT = 1000 * 20;     // [ms]
static constexpr int NETWORK_TIMEOUT = 1000 * 60 * 2;  // [ms]
static constexpr int CONNECT_TIMEOUT = 1000 * 10;      // [ms]
static constexpr int RECEIVE_TIMEOUT = 1000 * 10;      // [ms]

#define ABORT_IF_FAILED(result) \
  do { \
    if ((result) != WioCellularResult::Ok) abort(); \
  } while (0)

static void abortHandler(int sig) {
  while (true) {
    ledOn(LED_BUILTIN);
    delay(100);
    ledOff(LED_BUILTIN);
    delay(100);
  }
}

static SemaphoreHandle_t CellularWorkSem;
static SemaphoreHandle_t CellularStartSem;
static SemaphoreHandle_t MeasureSem;
static QueueSetHandle_t QueueSet;

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

  assert(CellularWorkSem = WioCellular.getInterface().getReceivedNotificationSemaphone());
  assert(CellularStartSem = xSemaphoreCreateBinary());
  assert(MeasureSem = xSemaphoreCreateBinary());

  assert(QueueSet = xQueueCreateSet(3));
  assert(xQueueAddToSet(CellularWorkSem, QueueSet) == pdPASS);
  assert(xQueueAddToSet(CellularStartSem, QueueSet) == pdPASS);
  assert(xQueueAddToSet(MeasureSem, QueueSet) == pdPASS);

  WioCellular.begin();
  ABORT_IF_FAILED(WioCellular.powerOn(POWER_ON_TIMEOUT));

  WioNetwork.config.searchAccessTechnology = SEARCH_ACCESS_TECHNOLOGY;
  WioNetwork.config.ltemBand = LTEM_BAND;
  WioNetwork.config.apn = APN;
  WioNetwork.begin();

  if (!WioCellular.doWork(NETWORK_TIMEOUT, [] {
        return WioNetwork.canCommunicate();
      })) abort();

  assert(xTimerStart(xTimerCreate("CellularStart", pdMS_TO_TICKS(START_DELAY), pdFALSE, CellularStartSem, semaphoreGiveTimerHandler), 0) == pdPASS);

  digitalWrite(LED_BUILTIN, LOW);
}

void loop(void) {
  yield();
  const auto activatedMember = xQueueSelectFromSet(QueueSet, portMAX_DELAY);
  assert(activatedMember);

  digitalWrite(LED_BUILTIN, HIGH);

  if (activatedMember == CellularWorkSem) {
    assert(xSemaphoreTake(activatedMember, 0) == pdTRUE);

    WioCellular.doWork(0);
  } else if (activatedMember == CellularStartSem) {
    assert(xSemaphoreTake(activatedMember, 0) == pdTRUE);

    ABORT_IF_FAILED(WioCellular.setPsmEnteringIndicationUrc(true));
    ABORT_IF_FAILED(WioCellular.setPsm(1, PSM_PERIOD, PSM_ACTIVE));

    assert(xTimerStart(xTimerCreate("Measure", pdMS_TO_TICKS(MEASURE_PERIOD), pdTRUE, MeasureSem, semaphoreGiveTimerHandler), 0) == pdPASS);
  } else if (activatedMember == MeasureSem) {
    assert(xSemaphoreTake(activatedMember, 0) == pdTRUE);

    JsonDoc.clear();
    if (measure(JsonDoc)) {
      ABORT_IF_FAILED(WioCellular.powerOn(POWER_ON_TIMEOUT));
      send(JsonDoc);
    }
  }

  digitalWrite(LED_BUILTIN, LOW);
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
    WioCellularTcpClient2<WioCellularModule> client{ WioCellular };
    if (!client.open(WioNetwork.config.pdpContextId, HOST, PORT)) {
      Serial.printf("ERROR: Failed to open %s\n", WioCellularResultToString(client.getLastResult()));
      return false;
    }

    if (!client.waitforConnect(CONNECT_TIMEOUT)) {
      Serial.printf("ERROR: Failed to connect %s\n", WioCellularResultToString(client.getLastResult()));
      return false;
    }

    Serial.print("Sending ");
    std::string str;
    serializeJson(doc, str);
    printData(Serial, str.data(), str.size());
    Serial.println();
    if (!client.send(str.data(), str.size())) {
      Serial.printf("ERROR: Failed to send socket %s\n", WioCellularResultToString(client.getLastResult()));
      return false;
    }

    Serial.println("Receiving");
    static uint8_t recvData[WioCellular.RECEIVE_SOCKET_SIZE_MAX];
    size_t recvSize;
    if (!client.receive(recvData, sizeof(recvData), &recvSize, RECEIVE_TIMEOUT)) {
      Serial.printf("ERROR: Failed to receive socket %s\n", WioCellularResultToString(client.getLastResult()));
      return false;
    }

    printData(Serial, recvData, recvSize);
    Serial.println();
  }

  Serial.println("### Completed");

  return true;
}

static void semaphoreGiveTimerHandler(TimerHandle_t timer) {
  xSemaphoreGive(pvTimerGetTimerID(timer));
}

template<typename T>
void printData(T& stream, const void* data, size_t size) {
  auto p = static_cast<const char*>(data);

  for (; size > 0; --size, ++p)
    stream.write(0x20 <= *p && *p <= 0x7f ? *p : '.');
}
