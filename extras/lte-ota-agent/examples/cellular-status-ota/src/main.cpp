/*
 * Based on WioCellular's cellular-status example.
 * Copyright (C) Seeed K.K.
 * MIT License
 */

#include <Adafruit_TinyUSB.h>
#include <Arduino.h>
#include <WioCellular.h>
#include <WioOtaAgent.h>

#ifndef APP_VERSION
#define APP_VERSION 1
#endif

namespace {

constexpr uint32_t kPowerOnTimeoutMs = 20UL * 1000UL;
constexpr uint32_t kNetworkTimeoutMs = 3UL * 60UL * 1000UL;
constexpr uint32_t kStatusIntervalMs = 5UL * 1000UL;

wio_ota_agent::Decision decideUpdate(
    const wio_ota_agent::Manifest& manifest) {
  return manifest.version > APP_VERSION
             ? wio_ota_agent::Decision::kApply
             : wio_ota_agent::Decision::kNoUpdate;
}

void reportProgress(size_t received, size_t total) {
  if ((received % (16 * 1024)) == 0 || received == total) {
    Serial.printf("[OTA] progress %u/%u\n",
                  static_cast<unsigned>(received),
                  static_cast<unsigned>(total));
  }
}

wio_ota_agent::Result checkOta() {
  wio_ota_agent::Config config;
  config.target_hardware = "wio-bg770a-v1.0";
  config.manifest_host = "metadata.soracom.io";
  config.manifest_path = "/v1/userdata";
  config.allowed_firmware_host = "harvest-files.soracom.io";
  config.pdp_context_id = WioNetwork.config.pdpContextId;

  static wio_ota_agent::Agent agent{WioCellular, config, &Serial};
  return agent.check(decideUpdate, reportProgress);
}

}  // namespace

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  Serial.begin(115200);
  const uint32_t serial_started_at = millis();
  while (!Serial && millis() - serial_started_at < 5000) {
    delay(10);
  }

  Serial.printf("cellular-status + OTA, app-version=%d\n", APP_VERSION);

  WioNetwork.config.apn = "soracom.io";
  WioCellular.begin();
  if (WioCellular.powerOn(kPowerOnTimeoutMs) != WioCellularResult::Ok) {
    Serial.println("[LTE] modem power-on failed");
    return;
  }

  WioNetwork.begin();
  if (!WioNetwork.waitUntilCommunicationAvailable(kNetworkTimeoutMs)) {
    Serial.println("[LTE] network attach failed");
    return;
  }

  Serial.println("[LTE] network ready");
  const auto result = checkOta();
  Serial.printf("[OTA] result=%s\n", wio_ota_agent::resultString(result));
}

void loop() {
  static uint32_t last_status_at = 0;
  WioCellular.doWorkUntil(100);
  if (millis() - last_status_at >= kStatusIntervalMs) {
    last_status_at = millis();
    digitalToggle(LED_BUILTIN);
    Serial.printf("[APP] version=%d uptime=%lu\n", APP_VERSION,
                  static_cast<unsigned long>(millis() / 1000));
  }
}
