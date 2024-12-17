/*
 * cellular-status.ino
 * Copyright (C) Seeed K.K.
 * MIT License
 */

#include <Adafruit_TinyUSB.h>
#include <WioCellular.h>

static constexpr int INTERVAL = 1000 * 5;           // [ms]
static constexpr int POWER_ON_TIMEOUT = 1000 * 20;  // [ms]

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

  WioCellular.begin();
  if (WioCellular.powerOn(POWER_ON_TIMEOUT) != WioCellularResult::Ok) abort();

  WioNetwork.begin();

  PrintInfo();
  Serial.println();
}

void loop(void) {
  PrintStatus();

  Serial.flush();
  WioCellular.doWorkUntil(INTERVAL);
}

static std::string RssiCodeToStr(int rssi) {
  if (rssi == 0) {
    return "~-113dBm";
  } else if (rssi == 1) {
    return "-111dBm";
  } else if (rssi <= 30) {
    const auto value = map(rssi, 2, 30, -109, -53);
    return std::to_string(value) + "dBm";
  } else if (rssi == 31) {
    return "-51~dBm";
  } else {
    return "Unknown";
  }
}

static std::string BerCodeToStr(int ber) {
  switch (ber) {
    case 0:
      return "0~0.2%";
    case 1:
      return "0.2~0.4%";
    case 2:
      return "0.4~0.8%";
    case 3:
      return "0.8~1.6%";
    case 4:
      return "1.6~3.2%";
    case 5:
      return "3.2~6.4%";
    case 6:
      return "6.4~12.8%";
    case 7:
      return "12.8~%";
    default:
      return "Unknown";
  }
}

static void PrintInfo(void) {
  std::string imei;
  WioCellular.getIMEI(&imei);
  std::string revision;
  WioCellular.getModemInfo(&revision);
  int simInserted;
  WioCellular.getSimInsertionStatus(nullptr, &simInserted);
  int simInitStatus;
  WioCellular.getSimInitializationStatus(&simInitStatus);
  std::string simState;
  WioCellular.getSimState(&simState);
  std::string imsi;
  WioCellular.getIMSI(&imsi);
  std::string iccid;
  WioCellular.getSimCCID(&iccid);
  std::string phoneNumber;
  WioCellular.getPhoneNumber(&phoneNumber);
  int searchAct;
  WioCellular.getSearchAccessTechnology(&searchAct);
  std::string searchActSeq;
  WioCellular.getSearchAccessTechnologySequence(&searchActSeq);
  std::string gsmBand;
  std::string emtcBand;
  std::string nbiotBand;
  WioCellular.getSearchFrequencyBand(&gsmBand, &emtcBand, &nbiotBand);

  Serial.printf("IMEI:                 %s\n", imei.c_str());
  Serial.printf("Revision:             %s\n", revision.c_str());
  Serial.printf("SIM Inserted:         %d(%s)\n", simInserted, simInserted == 0 ? "No" : simInserted == 1 ? "Yes"
                                                                                                          : "Unknown");
  Serial.printf("SIM Init:             %d(%s)\n", simInitStatus, simInitStatus == 0 ? "Initial" : simInitStatus == 1 ? "CPIN Ready"
                                                                                                : simInitStatus == 2 ? "SMS Done"
                                                                                                : simInitStatus == 3 ? "CPIN Ready & SMS Done"
                                                                                                                     : "Unknown");
  Serial.printf("SIM State:            %s\n", simState.c_str());
  Serial.printf("IMSI:                 %s\n", imsi.c_str());
  Serial.printf("ICCID:                %s\n", iccid.c_str());
  Serial.printf("Phone Number:         %s\n", phoneNumber.c_str());
  Serial.printf("Search ACT:           %d(%s)\n", searchAct, searchAct == 0 ? "eMTC" : searchAct == 1 ? "NB-IoT"
                                                                                     : searchAct == 2 ? "eMTC and NB-IoT"
                                                                                                      : "Unknown");
  Serial.printf("Search ACT Sequence:  %s(%s)\n", searchActSeq.c_str(), searchActSeq == "0203" ? "eMTC -> NB-IoT" : searchActSeq == "0302" ? "NB-IoT -> eMTC"
                                                                                                                                           : "Unknown");
  Serial.printf("Search Band - eMTC:   %s\n", emtcBand.c_str());
  Serial.printf("Search Band - NB-IoT: %s\n", nbiotBand.c_str());
}

static void PrintStatus(void) {
  const auto uptime = millis() / 1000;
  int rssi;
  int ber;
  WioCellular.getSignalQuality(&rssi, &ber);
  int state;
  WioCellular.getEpsNetworkRegistrationState(&state);
  int mode;
  int format;
  std::string oper;
  int act;
  WioCellular.getOperator(&mode, &format, &oper, &act);
  int psState;
  WioCellular.getPacketDomainState(&psState);

  Serial.printf("%u\t", uptime);
  Serial.printf("Status\t");
  Serial.printf("%d(%s)\t", rssi, RssiCodeToStr(rssi).c_str());
  Serial.printf("%d(%s)\t", ber, BerCodeToStr(ber).c_str());
  Serial.printf("%d(%s)\t", state,
                state == 0   ? "Not Registered"
                : state == 1 ? "Registered, Home Network"
                : state == 2 ? "Searching"
                : state == 3 ? "Denied"
                : state == 4 ? "Unknown"
                : state == 5 ? "Registered, Roaming"
                             : "Unknown");
  Serial.printf("%s, %d(%s)\t", oper.c_str(), act, act == 7 ? "eMTC" : act == 9 ? "NB-IoT"
                                                                                : "Unknown");
  Serial.printf("%d(%s)\n", psState, psState == 0 ? "Detached" : psState == 1 ? "Attached"
                                                                              : "Unknown");
}
