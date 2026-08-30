#include <Adafruit_TinyUSB.h>
#include <Arduino.h>
#include <WioCellular.h>
#include <WioOtaAgent.h>

#include <string>

#include "ota_config.h"

namespace {

constexpr uint32_t kPowerOnTimeoutMs = 20000;
constexpr uint32_t kNetworkTimeoutMs = 180000;
constexpr uint32_t kStatusIntervalMs = 5000;
constexpr char kTargetHardware[] = "wio-bg770a-v1.0";

const char* runtime_status = "booting";
char imsi_suffix[5] = "????";
uint32_t last_status_at = 0;

void printRuntimeStatus() {
  Serial.print("[STATUS] phase=");
  Serial.print(runtime_status);
  Serial.print(" sim=...");
  Serial.print(imsi_suffix);
  Serial.println();
}

bool ensurePdpContext() {
  std::vector<WioCellularModule::PdpContext> contexts;
  if (WioCellular.getPdpContext(&contexts) != WioCellularResult::Ok) {
    Serial.println("[LTE] failed to read PDP contexts");
    return false;
  }
  for (const auto& context : contexts) {
    if (context.cid == WioNetwork.config.pdpContextId &&
        context.apn == WIO_OTA_APN) {
      return true;
    }
  }

  int functionality = 0;
  if (WioCellular.getPhoneFunctionality(&functionality) !=
      WioCellularResult::Ok) {
    Serial.println("[LTE] failed to read phone functionality");
    return false;
  }
  if (functionality != 0 &&
      WioCellular.setPhoneFunctionality(0) != WioCellularResult::Ok) {
    Serial.println("[LTE] failed to disable phone functionality");
    return false;
  }
  const WioCellularResult set_result = WioCellular.setPdpContext(
      {WioNetwork.config.pdpContextId, "IP", WIO_OTA_APN, "0.0.0.0", 0, 0,
       0});
  const WioCellularResult restore_result =
      functionality != 0 ? WioCellular.setPhoneFunctionality(functionality)
                         : WioCellularResult::Ok;
  if (set_result != WioCellularResult::Ok ||
      restore_result != WioCellularResult::Ok) {
    Serial.println("[LTE] failed to configure PDP context");
    return false;
  }
  return true;
}

bool packetDomainReady() {
  int attached = -1;
  if (WioCellular.getPacketDomainState(&attached) != WioCellularResult::Ok) {
    Serial.println("[LTE] failed to read packet-domain attach state");
    return false;
  }

  std::vector<WioCellularModule::PdpContextStatus> statuses;
  if (WioCellular.getPdpContextStatus(&statuses) != WioCellularResult::Ok) {
    Serial.println("[LTE] failed to read PDP activation state");
    return false;
  }
  bool active = false;
  for (const auto& status : statuses) {
    if (status.cid == WioNetwork.config.pdpContextId && status.state == 1) {
      active = true;
      break;
    }
  }
  Serial.printf("[LTE] packet-domain attached=%d pdp-active=%d\n", attached,
                active ? 1 : 0);
  return attached == 1 && active;
}

bool configureSocketPdpContext() {
  char command[160] = {};
  snprintf(command, sizeof(command), "AT+QICSGP=%d,1,\"%s\",\"\",\"\",0",
           WioNetwork.config.pdpContextId, WIO_OTA_APN);
  if (WioCellular.executeCommand(command, 300) != WioCellularResult::Ok) {
    Serial.println("[LTE] failed to configure socket PDP context");
    return false;
  }
  Serial.println("[LTE] socket PDP context configured");
  return true;
}

bool activateSocketPdpContext() {
  char command[32] = {};
  snprintf(command, sizeof(command), "AT+QIACT=%d",
           WioNetwork.config.pdpContextId);
  if (WioCellular.executeCommand(command, 150000) == WioCellularResult::Ok) {
    Serial.println("[LTE] socket PDP context activated");
    return true;
  }

  bool active = false;
  const std::string active_prefix =
      "+QIACT: " + std::to_string(WioNetwork.config.pdpContextId) + ",1,";
  const WioCellularResult query_result = WioCellular.queryCommand(
      "AT+QIACT?",
      [&active, &active_prefix](const std::string& response) {
        if (response.compare(0, active_prefix.size(), active_prefix) == 0) {
          active = true;
        }
        return response.compare(0, 8, "+QIACT: ") == 0;
      },
      300);
  if (query_result == WioCellularResult::Ok && active) {
    Serial.println("[LTE] socket PDP context already active");
    return true;
  }
  Serial.println("[LTE] failed to activate socket PDP context");
  return false;
}

bool runSoracomPingDiagnostic() {
  std::vector<std::string> responses;
  const auto handler = WioCellular.registerUrcHandler2(
      [&responses](const std::string& response) {
        if (response.compare(0, 8, "+QPING: ") == 0) {
          responses.push_back(response);
          return true;
        }
        return false;
      });
  if (WioCellular.executeCommand(
          "AT+QPING=1,\"pong.soracom.io\",3,3", 300000) !=
      WioCellularResult::Ok) {
    Serial.println("[LTE] ping command failed");
    return false;
  }
  WioCellular.doWork(15000, [&responses] { return responses.size() >= 4; });
  Serial.printf("[LTE] ping responses=%u\n",
                static_cast<unsigned>(responses.size()));
  return responses.size() >= 4;
}

wio_ota_agent::Decision decideUpdate(
    const wio_ota_agent::Manifest& manifest) {
  if (manifest.version <= WIO_OTA_CURRENT_VERSION) {
    Serial.printf("[OTA] no update current=%d manifest=%lu\n",
                  WIO_OTA_CURRENT_VERSION,
                  static_cast<unsigned long>(manifest.version));
    return wio_ota_agent::Decision::kNoUpdate;
  }

#if WIO_OTA_AUTO_APPLY
  return wio_ota_agent::Decision::kApply;
#else
  return wio_ota_agent::Decision::kDownloadAndVerify;
#endif
}

void printOtaProgress(size_t received, size_t total) {
  if ((received % (16 * 1024)) == 0 || received == total) {
    Serial.printf("[OTA] progress %u/%u\n", static_cast<unsigned>(received),
                  static_cast<unsigned>(total));
  }
}

bool runOtaCheck() {
  wio_ota_agent::Config config;
  config.target_hardware = kTargetHardware;
  config.manifest_host = WIO_OTA_MANIFEST_HOST;
  config.manifest_port = WIO_OTA_MANIFEST_PORT;
  config.manifest_path = WIO_OTA_MANIFEST_PATH;
  config.allowed_firmware_host = WIO_OTA_FIRMWARE_HOST;
  config.allowed_firmware_port = WIO_OTA_FIRMWARE_PORT;
  config.pdp_context_id = WioNetwork.config.pdpContextId;
  static wio_ota_agent::Agent agent{WioCellular, config, &Serial};
  const wio_ota_agent::Result result =
      agent.check(decideUpdate, printOtaProgress);
  return result != wio_ota_agent::Result::kFailed &&
         result != wio_ota_agent::Result::kRejected;
}

}  // namespace

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  Serial.begin(115200);
  const uint32_t wait_started = millis();
  while (!Serial && millis() - wait_started < 5000) {
    delay(10);
  }
  Serial.printf("WIO OTA LTE agent app-version=%d (HW 1.0)\n",
                WIO_OTA_CURRENT_VERSION);

#if WIO_OTA_ENABLED
  runtime_status = "modem-power-on";
  // Preserve the deployed modem's RAT and band settings. Forcing a change here
  // can make Bg770aNetwork::begin() wait indefinitely for a NotSearching URC
  // after CFUN=0. The OTA agent only needs to ensure the Soracom APN exists.
  WioNetwork.config.apn = WIO_OTA_APN;
  WioCellular.begin();

  if (WioCellular.powerOn(kPowerOnTimeoutMs) != WioCellularResult::Ok) {
    runtime_status = "modem-power-failed";
    Serial.println("[LTE] modem power-on failed");
    return;
  }
  std::string imsi;
  if (WioCellular.getIMSI(&imsi) == WioCellularResult::Ok &&
      imsi.size() >= 4) {
    memcpy(imsi_suffix, imsi.c_str() + imsi.size() - 4, 4);
    imsi_suffix[4] = '\0';
    Serial.print("[LTE] SIM ...");
    Serial.println(imsi_suffix);
  } else {
    Serial.println("[LTE] SIM identity unavailable");
  }
  // CPSMS changes take effect on the next network registration. The target
  // group grants a short PSM active timer, so detach before disabling PSM and
  // then perform a fresh attach; otherwise HTTP commands may stop echoing.
  if (WioCellular.setPhoneFunctionality(0) != WioCellularResult::Ok) {
    runtime_status = "radio-detach-failed";
    Serial.println("[LTE] failed to detach before disabling PSM");
    return;
  }
  Serial.println("[LTE] radio detached for OTA setup");
  if (WioCellular.setPsm(0, 120, 16) != WioCellularResult::Ok) {
    runtime_status = "psm-disable-failed";
    Serial.println("[LTE] failed to disable PSM for OTA");
    return;
  }
  Serial.println("[LTE] PSM disabled for OTA");
  runtime_status = "network-attach";
  if (!ensurePdpContext()) {
    runtime_status = "pdp-context-failed";
    return;
  }
  Serial.println("[LTE] PDP context ready");
  if (!configureSocketPdpContext()) {
    runtime_status = "socket-pdp-config-failed";
    return;
  }
  if (WioCellular.setPhoneFunctionality(1) != WioCellularResult::Ok) {
    runtime_status = "radio-attach-failed";
    Serial.println("[LTE] failed to restart radio after OTA setup");
    return;
  }
  Serial.println("[LTE] radio restarted for fresh attach");
  WioNetwork.begin();
  Serial.println("[LTE] network helper initialized");
  if (!WioNetwork.waitUntilCommunicationAvailable(kNetworkTimeoutMs)) {
    runtime_status = "network-attach-failed";
    Serial.println("[LTE] network attach failed");
    return;
  }
  runtime_status = "ota-check";
  Serial.println("[LTE] network ready");
  if (!packetDomainReady()) {
    runtime_status = "packet-domain-not-ready";
    Serial.println("[LTE] packet domain is not ready for sockets");
    return;
  }
  if (!activateSocketPdpContext()) {
    runtime_status = "socket-pdp-activate-failed";
    return;
  }
  if (!runSoracomPingDiagnostic()) {
    runtime_status = "soracom-ping-failed";
    return;
  }
  runtime_status = runOtaCheck() ? "ota-check-complete" : "ota-check-failed";
#else
  runtime_status = "ota-disabled";
  Serial.println(
      "[OTA] disabled; copy include/ota_config.local.h.example to "
      "include/ota_config.local.h and configure it");
#endif
}

void loop() {
#if WIO_OTA_ENABLED
  WioCellular.doWorkUntil(1000);
#else
  delay(1000);
#endif
  if (millis() - last_status_at >= kStatusIntervalMs) {
    last_status_at = millis();
    printRuntimeStatus();
  }
}
