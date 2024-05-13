// Tasmota HAN Driver for EMI (edpbox)
// easyhan.pt

#define HAN_VERSION "13.4.0-7.21.3"

#ifdef USE_HAN_V2

#warning **** HAN_V2 Driver is included... ****

#define XDRV_100 100

// This variable will be set to true after initialization
bool hDrvInit = false;

// HAN

uint8_t hanCFG = 99;
uint8_t hanEB = 99;
uint8_t hanERR = 0;
bool hanWork = false;
uint32_t hanDelay = 0;
uint32_t hanDelayWait = 1100;
uint32_t hanDelayError = 40000;
uint8_t hanIndex = 0;  // 0 = setup
uint32_t hanRead = 0;
uint8_t hanCode = 0;

// Clock 01
uint16_t hanYY = 0;
uint8_t hanMT = 0;
uint8_t hanDD = 0;

uint8_t hanHH = 0;
uint8_t hanMM = 0;
uint8_t hanSS = 0;

// Voltage Current 6C
// mono
float hanVL1 = 0;
float hanCL1 = 0;
// tri
float hanVL2 = 0;
float hanCL2 = 0;
float hanVL3 = 0;
float hanCL3 = 0;
float hanCLT = 0;

// Total Energy T (kWh) 26
float hanTET1 = 0;
float hanTET2 = 0;
float hanTET3 = 0;

// Total Energy (kWh) 16
float hanTEI = 0;
float hanTEE = 0;

// Total Energy L1 L2 L3 (kWh) 1C
float hTEIL1 = 0;
float hTEIL2 = 0;
float hTEIL3 = 0;

// Active Power Import/Export 73
// tri
uint32_t hanAPI1 = 0;
uint32_t hanAPE1 = 0;
uint32_t hanAPI2 = 0;
uint32_t hanAPE2 = 0;
uint32_t hanAPI3 = 0;
uint32_t hanAPE3 = 0;
// mono / tri total. (79)
uint32_t hanAPI = 0;
uint32_t hanAPE = 0;

// Power Factor (7B) / Frequency (7F)
float hanPF = 0;
float hanPF1 = 0;
float hanPF2 = 0;
float hanPF3 = 0;
float hanFR = 0;

// Load Profile

uint16_t hLP1YY = 0;
uint8_t hLP1MT = 0;
uint8_t hLP1DD = 0;
uint8_t hLP1HH = 0;
uint8_t hLP1MM = 0;
char hLP1gmt[5];

uint16_t hLP2 = 0;  // tweaked to 16bits

float hLP3 = 0;
float hLP4 = 0;
float hLP5 = 0;
float hLP6 = 0;
float hLP7 = 0;
float hLP8 = 0;
uint8_t hLPid[9];
uint32_t hLPX[2];

// Misc

float hCT1 = 0;
uint8_t hTariff = 0;
char hErrTime[10];
char hErrCode[8];
uint8_t hErrIdx = 0;
char hStatus[10];
uint32_t hPerf[2] = {0, 0};
uint32_t hMnfC = 0;
uint16_t hMnfY = 0;
char hFw1[10];
char hFw2[10];
char hFw3[10];

// **********************

#include <HardwareSerial.h>
#include <ModbusMaster.h>

#define MAX485_DE_RE 16

#ifdef ESP8266
HardwareSerial &HanSerial = Serial;
#endif

ModbusMaster node;

void preTransmission() { digitalWrite(MAX485_DE_RE, 1); }

void postTransmission() { digitalWrite(MAX485_DE_RE, 0); }

void hanBlink() {
#ifdef ESP8266
  digitalWrite(2, LOW);
  delay(50);
  digitalWrite(2, HIGH);
#endif
}

void setDelayError(uint8_t hanRes) {
  sprintf(hStatus, "Error");
  hanCode = hanRes;
  //
  hanDelay = hanDelayError;
  //
  sprintf(hErrTime, "%02d:%02d:%02d", hanHH, hanMM,
          hanSS);
  hErrIdx = hanIndex;
  sprintf(hErrCode, "0x%02X-%d", hanCode, hErrIdx);
  //
  hanIndex = 0;  // back to setup
}

void HanInit() {
  AddLog(LOG_LEVEL_INFO, PSTR("HAN: Init..."));

  if (PinUsed(GPIO_MBR_RX) | PinUsed(GPIO_MBR_TX) |
      PinUsed(GPIO_TCP_RX) | PinUsed(GPIO_TCP_TX)) {
    AddLog(LOG_LEVEL_INFO,
           PSTR("HAN: Driver disabled. Bridge Mode..."));
  } else {
#ifdef ESP8266
    //
    pinMode(2, OUTPUT);
    digitalWrite(2, LOW);
    //
    pinMode(MAX485_DE_RE, OUTPUT);
    digitalWrite(MAX485_DE_RE, LOW);
    //
#endif

    ClaimSerial();  // Tasmota SerialLog

    sprintf(hStatus, "Init");
    hanRead = millis() + 5000;

    // Init is successful
    hDrvInit = true;
  }
}  // end HanInit

void HanDoWork(void) {
  //

  if (hanRead + hanDelay < millis()) {
    hanWork = true;
    node.clearTransmitBuffer();
    delay(50);
    node.clearResponseBuffer();
    delay(50);
    node.setTimeout(750);
  }

  // # # # # # # # # # #
  // EASYHAN MODBUS BEGIN
  // # # # # # # # # # #

  uint8_t hRes;

  // # # # # # # # # # #
  // Setup: Serial
  // # # # # # # # # # #

  if (hanWork & (hanIndex == 0)) {
    //
    // Detect Stop Bits

    node.setTimeout(2000);

    HanSerial.flush();
    HanSerial.end();
    delay(250);
    HanSerial.begin(9600, SERIAL_8N1);
    delay(250);
    node.begin(1, HanSerial);
    node.preTransmission(preTransmission);
    node.postTransmission(postTransmission);

    delay(100);

    uint8_t testserial;

    node.clearResponseBuffer();
    testserial = node.readInputRegisters(0x0001, 1);
    if ((testserial == 0x00) | (testserial == 0x81)) {
      hanCFG = 1;
      hanIndex++;
      hanDelay = hanDelayWait;
      AddLog(LOG_LEVEL_INFO, PSTR("HAN: *** 8N1 OK ***"));
    } else {
      hanCode = testserial;
      AddLog(LOG_LEVEL_INFO,
             PSTR("HAN: 8N1 Fail. Error %d"), hanCode);
      //
      HanSerial.flush();
      HanSerial.end();
      delay(250);
      HanSerial.begin(9600, SERIAL_8N2);
      delay(250);
      node.begin(1, HanSerial);
      node.preTransmission(preTransmission);
      node.postTransmission(postTransmission);

      delay(100);
      //
      node.clearResponseBuffer();
      testserial = node.readInputRegisters(0x0001, 1);
      if ((testserial == 0x00) | (testserial == 0x81)) {
        hanCFG = 2;
        hanIndex++;
        hanDelay = hanDelayWait;
        AddLog(LOG_LEVEL_INFO,
               PSTR("HAN: *** 8N2 OK ***"));
      } else {
        AddLog(LOG_LEVEL_INFO,
               PSTR("HAN: 8N2 Fail. Error %d"), hanCode);
        setDelayError(testserial);
      }
      //
    }
    //
    hanRead = millis();
    hanWork = false;
  }

  // # # # # # # # # # #
  // Setup: EB
  // # # # # # # # # # #

  if (hanWork & (hanIndex == 1)) {
    //
    // Detect EB Type

    uint8_t testEB;
    uint16_t hanDTT = 0;

    node.clearResponseBuffer();
    testEB = node.readInputRegisters(0x0070, 2);
    if (testEB == node.ku8MBSuccess) {
      //
      hanDTT = node.getResponseBuffer(0);
      if (hanDTT > 0) {
        hanEB = 3;
        AddLog(LOG_LEVEL_INFO, PSTR("HAN: *** EB3 ***"));
      } else {
        hanEB = 1;
        AddLog(LOG_LEVEL_INFO, PSTR("HAN: *** EB1 ***"));
      }
      //
    } else {
      hanEB = 1;
      AddLog(LOG_LEVEL_INFO, PSTR("HAN: *** EB1 ***"));
    }
    hanIndex++;
    hanRead = millis();
    hanWork = false;
  }

  // # # # # # # # # # #
  // Contract
  // # # # # # # # # # #

  if (hanWork & (hanIndex == 2)) {
    hRes = node.readInputRegisters(0x000C, 1);
    if (hRes == node.ku8MBSuccess) {
      hCT1 = (node.getResponseBuffer(1) |
              node.getResponseBuffer(0) << 16) /
             1000.0;
      hanBlink();
      hanDelay = hanDelayWait;
      hanIndex++;
    } else {
      hanERR++;
      setDelayError(hRes);
    }
    hanRead = millis();
    hanWork = false;
  }

  // # # # # # # # # # #
  // LP ID
  // # # # # # # # # # #

  if (hanWork & (hanIndex == 3)) {
    hRes = node.readInputRegisters(0x0080, 1);
    if (hRes == node.ku8MBSuccess) {
      hLPid[1] = node.getResponseBuffer(0) >> 8;
      hLPid[2] = node.getResponseBuffer(0) & 0xFF;
      hLPid[3] = node.getResponseBuffer(1) >> 8;
      hLPid[4] = node.getResponseBuffer(1) & 0xFF;
      hLPid[5] = node.getResponseBuffer(2) >> 8;
      hLPid[6] = node.getResponseBuffer(2) & 0xFF;
      hLPid[7] = node.getResponseBuffer(3) >> 8;
      hLPid[8] = node.getResponseBuffer(3) & 0xFF;

      hanBlink();
      hanDelay = hanDelayWait;
      hanIndex++;
    } else {
      hanERR++;
      setDelayError(hRes);
    }
    hanRead = millis();
    hanWork = false;
  }

  // # # # # # # # # # #
  // EMI Info
  // # # # # # # # # # #

  if (hanWork & (hanIndex == 4)) {
    hRes = node.readInputRegisters(0x0003, 4);
    if (hRes == node.ku8MBSuccess) {
      hMnfC = node.getResponseBuffer(1) |
              node.getResponseBuffer(0) << 16;
      hMnfY = node.getResponseBuffer(2);

      sprintf(hFw1, "%c%c%c%c%c",
              node.getResponseBuffer(3) >> 8,
              node.getResponseBuffer(3) & 0xFF,
              node.getResponseBuffer(4) >> 8,
              node.getResponseBuffer(4) & 0xFF,
              node.getResponseBuffer(5) >> 8);

      sprintf(hFw2, "%c%c%c%c%c",
              node.getResponseBuffer(5) & 0xFF,
              node.getResponseBuffer(6) >> 8,
              node.getResponseBuffer(6) & 0xFF,
              node.getResponseBuffer(7) >> 8,
              node.getResponseBuffer(7) & 0xFF);

      sprintf(hFw3, "%c%c%c%c%c",
              node.getResponseBuffer(8) >> 8,
              node.getResponseBuffer(8) & 0xFF,
              node.getResponseBuffer(9) >> 8,
              node.getResponseBuffer(9) & 0xFF,
              node.getResponseBuffer(10) >> 8);

      hanBlink();
      hanDelay = hanDelayWait;
      hanIndex++;
    } else {
      hanERR++;
      setDelayError(hRes);
    }
    hanRead = millis();
    hanWork = false;
  }

  // # # # # # # # # # #
  // Clock ( 12 bytes )
  // # # # # # # # # # #

  if (hanWork & (hanIndex == 5)) {
    hRes = node.readInputRegisters(0x0001, 1);
    if (hRes == node.ku8MBSuccess) {
      hanYY = node.getResponseBuffer(0);
      hanMT = node.getResponseBuffer(1) >> 8;
      hanDD = node.getResponseBuffer(1) & 0xFF;
      // hanWD = node.getResponseBuffer(2) >> 8;
      hanHH = node.getResponseBuffer(2) & 0xFF;
      hanMM = node.getResponseBuffer(3) >> 8;
      hanSS = node.getResponseBuffer(3) & 0xFF;
      hanBlink();
      hanDelay = hanDelayWait;
      hanIndex++;
      AddLog(LOG_LEVEL_INFO,
             PSTR("HAN: %02d:%02d:%02d !"), hanHH, hanMM,
             hanSS);
      sprintf(hStatus, "OK");
    } else {
      hanERR++;
      setDelayError(hRes);
    }
    hanRead = millis();
    hanWork = false;
  }

  // # # # # # # # # # #
  // Voltage Current
  // # # # # # # # # # #

  if (hanWork & (hanIndex == 6)) {
    if (hanEB == 3) {
      hRes = node.readInputRegisters(0x006c, 7);
      if (hRes == node.ku8MBSuccess) {
        hanVL1 = node.getResponseBuffer(0) / 10.0;
        hanCL1 = node.getResponseBuffer(1) / 10.0;
        hanVL2 = node.getResponseBuffer(2) / 10.0;
        hanCL2 = node.getResponseBuffer(3) / 10.0;
        hanVL3 = node.getResponseBuffer(4) / 10.0;
        hanCL3 = node.getResponseBuffer(5) / 10.0;
        hanCLT = node.getResponseBuffer(6) / 10.0;
        hanBlink();
        hanDelay = hanDelayWait;
        hanIndex++;
      } else {
        hanERR++;
        setDelayError(hRes);
      }
    } else {
      hRes = node.readInputRegisters(0x006c, 2);
      if (hRes == node.ku8MBSuccess) {
        hanVL1 = node.getResponseBuffer(0) / 10.0;
        hanCL1 = node.getResponseBuffer(1) / 10.0;
        hanBlink();
        hanDelay = hanDelayWait;
        hanIndex++;
      } else {
        hanERR++;
        setDelayError(hRes);
      }
    }
    hanRead = millis();
    hanWork = false;
  }

  // # # # # # # # # # #
  // Active Power Import/Export 73 (tri)
  // Power Factor (mono) (79..)
  // # # # # # # # # # #

  if (hanWork & ((hanIndex == 7) | (hanIndex == 12))) {
    if (hanEB == 3) {
      hRes = node.readInputRegisters(0x0073, 8);
      if (hRes == node.ku8MBSuccess) {
        hanAPI1 = node.getResponseBuffer(1) |
                  node.getResponseBuffer(0) << 16;
        hanAPE1 = node.getResponseBuffer(3) |
                  node.getResponseBuffer(2) << 16;
        hanAPI2 = node.getResponseBuffer(5) |
                  node.getResponseBuffer(4) << 16;
        hanAPE2 = node.getResponseBuffer(7) |
                  node.getResponseBuffer(6) << 16;
        hanAPI3 = node.getResponseBuffer(9) |
                  node.getResponseBuffer(8) << 16;
        hanAPE3 = node.getResponseBuffer(11) |
                  node.getResponseBuffer(10) << 16;
        hanAPI = node.getResponseBuffer(13) |
                 node.getResponseBuffer(12) << 16;
        hanAPE = node.getResponseBuffer(15) |
                 node.getResponseBuffer(14) << 16;
        hanBlink();
        hanDelay = hanDelayWait;
        hanIndex++;
      } else {
        hanERR++;
        setDelayError(hRes);
      }
    } else {
      hRes = node.readInputRegisters(0x0079, 3);
      if (hRes == node.ku8MBSuccess) {
        hanAPI = node.getResponseBuffer(1) |
                 node.getResponseBuffer(0) << 16;
        hanAPE = node.getResponseBuffer(3) |
                 node.getResponseBuffer(2) << 16;
        hanPF = node.getResponseBuffer(4) / 1000.0;
        hanBlink();
        hanDelay = hanDelayWait;
        hanIndex++;
      } else {
        hanERR++;
        setDelayError(hRes);
      }
    }
    //
    AddLog(LOG_LEVEL_INFO,
           PSTR("HAN: Import %d Export %d"), hanAPI,
           hanAPE);
    //
    hanRead = millis();
    hanWork = false;
  }

  // # # # # # # # # # #
  // Power Factor (7B) / Frequency (7F)
  // Power Factor (tri)
  // Frequency (mono)
  // # # # # # # # # # #

  if (hanWork & (hanIndex == 8)) {
    if (hanEB == 3) {
      hRes = node.readInputRegisters(0x007b, 5);
      if (hRes == node.ku8MBSuccess) {
        hanPF = node.getResponseBuffer(0) / 1000.0;
        hanPF1 = node.getResponseBuffer(1) / 1000.0;
        hanPF2 = node.getResponseBuffer(2) / 1000.0;
        hanPF3 = node.getResponseBuffer(3) / 1000.0;
        hanFR = node.getResponseBuffer(4) / 10.0;
        hanBlink();
        hanDelay = hanDelayWait;
        hanIndex++;
      } else {
        hanERR++;
        setDelayError(hRes);
      }
    } else {
      hRes = node.readInputRegisters(0x007f, 1);
      if (hRes == node.ku8MBSuccess) {
        hanFR = node.getResponseBuffer(0) / 10.0;
        hanBlink();
        hanDelay = hanDelayWait;
        hanIndex++;
      } else {
        hanERR++;
        setDelayError(hRes);
      }
    }
    hanRead = millis();
    hanWork = false;
  }

  // # # # # # # # # # #
  // Total Energy Tarifas (kWh) 26
  // # # # # # # # # # #

  if (hanWork & (hanIndex == 9)) {
    hPerf[0] = millis();
    hRes = node.readInputRegisters(0x0026, 3);
    if (hRes == node.ku8MBSuccess) {
      hPerf[1] = millis() - hPerf[0];
      hanTET1 = (node.getResponseBuffer(1) |
                 node.getResponseBuffer(0) << 16) /
                1000.0;
      hanTET2 = (node.getResponseBuffer(3) |
                 node.getResponseBuffer(2) << 16) /
                1000.0;
      hanTET3 = (node.getResponseBuffer(5) |
                 node.getResponseBuffer(4) << 16) /
                1000.0;
      hanBlink();
      hanDelay = hanDelayWait;
      hanIndex++;
      sprintf(hStatus, "OK");
    } else {
      hanERR++;
      setDelayError(hRes);
    }
    hanRead = millis();
    hanWork = false;
  }

  // # # # # # # # # # #
  // Total Energy (total) (kWh) 16
  // # # # # # # # # # #

  if (hanWork & (hanIndex == 10)) {
    hRes = node.readInputRegisters(0x0016, 2);
    if (hRes == node.ku8MBSuccess) {
      hanTEI = (node.getResponseBuffer(1) |
                node.getResponseBuffer(0) << 16) /
               1000.0;
      hanTEE = (node.getResponseBuffer(3) |
                node.getResponseBuffer(2) << 16) /
               1000.0;
      hanBlink();
      hanDelay = hanDelayWait;
      hanIndex++;
    } else {
      hanERR++;
      setDelayError(hRes);
    }
    hanRead = millis();
    hanWork = false;
    //
    if (hanEB == 1) {
      hanIndex++;
    }
    //
  }

  // # # # # # # # # # #
  // Total Energy (L1 L2 L3) (kWh) 1C
  // # # # # # # # # # #

  if (hanWork & (hanIndex == 11)) {
    hRes = node.readInputRegisters(0x001C, 3);
    if (hRes == node.ku8MBSuccess) {
      hTEIL1 = (node.getResponseBuffer(1) |
                node.getResponseBuffer(0) << 16) /
               1000.0;
      hTEIL2 = (node.getResponseBuffer(3) |
                node.getResponseBuffer(2) << 16) /
               1000.0;
      hTEIL3 = (node.getResponseBuffer(5) |
                node.getResponseBuffer(4) << 16) /
               1000.0;
      hanBlink();
      hanDelay = hanDelayWait;
      hanIndex++;
    } else {
      hanERR++;
      setDelayError(hRes);
    }
    hanRead = millis();
    hanWork = false;
  }

  // # # # # # # # # # #
  // Reserved
  // # # # # # # # # # #

  // hanIndex == 12

  // # # # # # # # # # #
  // Load Profile
  // # # # # # # # # # #

  if (hanWork & (hanIndex == 13)) {
    //
    node.setTimeout(2000);
    //
    hPerf[0] = millis();
    hRes = node.readLastProfile(0x00, 0x01);
    if (hRes == node.ku8MBSuccess) {
      hPerf[1] = millis() - hPerf[0];
      hLP1YY = node.getResponseBuffer(0);
      hLP1MT = node.getResponseBuffer(1) >> 8;
      hLP1DD = node.getResponseBuffer(1) & 0xFF;

      hLP1HH = node.getResponseBuffer(2) & 0xFF;
      hLP1MM = node.getResponseBuffer(3) >> 8;

      uint8_t tmpGMT = node.getResponseBuffer(5) & 0xFF;

      // tweaked to 16bits. branch: LP1.
      hLP2 = node.getResponseBuffer(6);

      hLP3 = (node.getResponseBuffer(8) |
              node.getResponseBuffer(7) << 16) /
             1000.0;
      hLP4 = (node.getResponseBuffer(10) |
              node.getResponseBuffer(9) << 16) /
             1000.0;
      hLP5 = (node.getResponseBuffer(12) |
              node.getResponseBuffer(11) << 16) /
             1000.0;
      hLP6 = (node.getResponseBuffer(14) |
              node.getResponseBuffer(13) << 16) /
             1000.0;
      hLP7 = (node.getResponseBuffer(16) |
              node.getResponseBuffer(15) << 16) /
             1000.0;
      hLP8 = (node.getResponseBuffer(18) |
              node.getResponseBuffer(17) << 16) /
             1000.0;

      if (tmpGMT == 0x80) {
        sprintf(hLP1gmt, "01");
      } else {
        sprintf(hLP1gmt, "00");
      }

      hanBlink();
      hanDelay = hanDelayWait;
      hanIndex++;
    } else {
      hanERR++;
      setDelayError(hRes);
    }
    hanRead = millis();
    hanWork = false;
  }

  // # # # # # # # # # #
  // Tariff
  // # # # # # # # # # #

  if (hanWork & (hanIndex == 14)) {
    hRes = node.readInputRegisters(0x000B, 1);
    if (hRes == node.ku8MBSuccess) {
      hTariff = node.getResponseBuffer(0) >> 8;
      hanBlink();
      hanDelay = hanDelayWait;
      hanIndex++;
    } else {
      hanERR++;
      setDelayError(hRes);
    }
    hanRead = millis();
    hanWork = false;
  }

  // # # # # # # # # # #
  // EASYHAN MODBUS EOF
  // # # # # # # # # # #

  if (hanIndex > 14) {
    hanIndex = 5;  // skip setup and one time requests.
  }

  if (hanERR > 90) {
    hanERR = 10;
    hanIndex = 0;  // back to setup
  }

  // end loop

}  // end HanDoWork

void HanJson(bool json) {
  //
  char hanClock[10];
  sprintf(hanClock, "%02d:%02d:%02d", hanHH, hanMM,
          hanSS);
  //
  if (json) {
    ResponseAppend_P(",\"EB%d\":{", hanEB);
    ResponseAppend_P("\"ErrCode\":\"%s\"", hErrCode);
    ResponseAppend_P(",\"ErrTime\":\"%s\"", hErrTime);
    ResponseAppend_P(",\"ErrCnt\":%d", hanERR);

    ResponseAppend_P(",\"CLK\":\"%s\"", hanClock);
    ResponseAppend_P(",\"HH\":%d", hanHH);
    ResponseAppend_P(",\"MM\":%d", hanMM);
    ResponseAppend_P(",\"SS\":%d", hanSS);

    ResponseAppend_P(",\"VL1\":%1_f", &hanVL1);

    if (hanEB == 3) {
      ResponseAppend_P(",\"VL2\":%1_f", &hanVL2);
      ResponseAppend_P(",\"VL3\":%1_f", &hanVL3);
    }

    ResponseAppend_P(",\"CL1\":%1_f", &hanCL1);

    if (hanEB == 3) {
      ResponseAppend_P(",\"CL2\":%1_f", &hanCL2);
      ResponseAppend_P(",\"CL3\":%1_f", &hanCL3);
      ResponseAppend_P(",\"CL\":%1_f", &hanCLT);
    }

    ResponseAppend_P(",\"API\":%d", hanAPI);
    ResponseAppend_P(",\"APE\":%d", hanAPE);

    if (hanEB == 3) {
      ResponseAppend_P(",\"API1\":%d", hanAPI1);
      ResponseAppend_P(",\"API2\":%d", hanAPI2);
      ResponseAppend_P(",\"API3\":%d", hanAPI3);
      ResponseAppend_P(",\"APE1\":%d", hanAPE1);
      ResponseAppend_P(",\"APE2\":%d", hanAPE2);
      ResponseAppend_P(",\"APE3\":%d", hanAPE3);
    }

    ResponseAppend_P(",\"PF\":%3_f", &hanPF);

    if (hanEB == 3) {
      ResponseAppend_P(",\"PF1\":%3_f", &hanPF1);
      ResponseAppend_P(",\"PF2\":%3_f", &hanPF2);
      ResponseAppend_P(",\"PF3\":%3_f", &hanPF3);
    }

    ResponseAppend_P(",\"FR\":%1_f", &hanFR);

    ResponseAppend_P(",\"TET1\":%3_f", &hanTET1);
    ResponseAppend_P(",\"TET2\":%3_f", &hanTET2);
    ResponseAppend_P(",\"TET3\":%3_f", &hanTET3);

    ResponseAppend_P(",\"TEI\":%3_f", &hanTEI);
    ResponseAppend_P(",\"TEE\":%3_f", &hanTEE);

    if (hanEB == 3) {
      ResponseAppend_P(",\"TEIL1\":%3_f", &hTEIL1);
      ResponseAppend_P(",\"TEIL2\":%3_f", &hTEIL2);
      ResponseAppend_P(",\"TEIL3\":%3_f", &hTEIL3);
    }

    ResponseAppend_P(",\"LP1_Y\":%d", hLP1YY);
    ResponseAppend_P(",\"LP1_M\":%d", hLP1MT);
    ResponseAppend_P(",\"LP1_D\":%d", hLP1DD);
    ResponseAppend_P(",\"LP1_HH\":%d", hLP1HH);
    ResponseAppend_P(",\"LP1_MM\":%d", hLP1MM);
    ResponseAppend_P(",\"LP1_GMT\":\"%s\"", hLP1gmt);

    ResponseAppend_P(",\"LP3_IMP\":%3_f", &hLP3);
    ResponseAppend_P(",\"LP4\":%3_f", &hLP4);
    ResponseAppend_P(",\"LP5\":%3_f", &hLP5);
    ResponseAppend_P(",\"LP6_EXP\":%3_f", &hLP6);

    ResponseAppend_P(",\"CT1\":%2_f", &hCT1);
    ResponseAppend_P(",\"Tariff\":%d", hTariff);

    ResponseAppend_P("}");

  } else {
    WSContentSend_PD("{s}<br>{m} {e}");
    WSContentSend_PD("{s}HAN V2 " HAN_VERSION " {m} {e}");
    WSContentSend_PD("{s}<br>{m} {e}");

    uint32_t tmpWait =
        ((hanRead + hanDelay) - millis()) / 1000;

    if (tmpWait > 900) {
      tmpWait = 999;
    }

    WSContentSend_PD("{s}MB Status {m} %s %ds {e}",
                     hStatus, tmpWait);

    WSContentSend_PD("{s}MB Index {m} %d {e}", hanIndex);

    if (hanERR > 0) {
      WSContentSend_PD("{s}MB Error Time {m} %s{e}",
                       hErrTime);
      WSContentSend_PD("{s}MB Error Code {m} %s {e}",
                       hErrCode);
      WSContentSend_PD("{s}MB Error Count {m} %d {e}",
                       hanERR);
    }

    WSContentSend_PD("{s}MB Serial {m} 8N%d {e}", hanCFG);
    WSContentSend_PD("{s}MB Type {m} EB%d {e}", hanEB);
    WSContentSend_PD("{s}MB Latency {m} %d ms{e}",
                     hPerf[1]);
    WSContentSend_PD("{s}MB Delay Wait {m} %d ms{e}",
                     hanDelayWait);
    WSContentSend_PD("{s}MB Delay Error {m} %d ms{e}",
                     hanDelayError);
    WSContentSend_PD("{s}<br>{m} {e}");

    WSContentSend_PD("{s}Clock {m} %s{e}", hanClock);

    WSContentSend_PD("{s}<br>{m} {e}");

    WSContentSend_PD("{s}Voltage L1 {m} %1_f V{e}",
                     &hanVL1);

    if (hanEB == 3) {
      WSContentSend_PD("{s}L2 {m} %1_f V{e}", &hanVL2);
      WSContentSend_PD("{s}L3 {m} %1_f V{e}", &hanVL3);

      WSContentSend_PD("{s}<br>{m} {e}");
      WSContentSend_PD("{s}Current{m} %1_f A{e}",
                       &hanCLT);
    }

    WSContentSend_PD("{s}Current L1 {m} %1_f A{e}",
                     &hanCL1);

    if (hanEB == 3) {
      WSContentSend_PD("{s}L2 {m} %1_f A{e}", &hanCL2);
      WSContentSend_PD("{s}L3 {m} %1_f A{e}", &hanCL3);

      WSContentSend_PD("{s}<br>{m} {e}");
    }

    WSContentSend_PD("{s}Power Import {m} %d W{e}",
                     hanAPI);
    WSContentSend_PD("{s}Power Export {m} %d W{e}",
                     hanAPE);

    if (hanEB == 3) {
      WSContentSend_PD("{s}L1 {m} %d W{e}", hanAPI1);
      WSContentSend_PD("{s}L2 {m} %d W{e}", hanAPI2);
      WSContentSend_PD("{s}L3 {m} %d W{e}", hanAPI3);
      WSContentSend_PD("{s}L1 Export {m} %d W{e}",
                       hanAPE1);
      WSContentSend_PD("{s}L2 Export {m} %d W{e}",
                       hanAPE2);
      WSContentSend_PD("{s}L3 Export {m} %d W{e}",
                       hanAPE3);

      WSContentSend_PD("{s}<br>{m} {e}");
    }

    WSContentSend_PD("{s}Power Factor {m} %3_f φ{e}",
                     &hanPF);

    if (hanEB == 3) {
      WSContentSend_PD("{s}L1 {m} %3_f φ{e}", &hanPF1);
      WSContentSend_PD("{s}L2 {m} %3_f φ{e}", &hanPF2);
      WSContentSend_PD("{s}L3 {m} %3_f φ{e}", &hanPF3);
    }

    WSContentSend_PD("{s}Frequency {m} %1_f Hz{e}",
                     &hanFR);

    WSContentSend_PD("{s}<br>{m} {e}");

    WSContentSend_PD("{s}Energy T1 Vazio {m} %3_f kWh{e}",
                     &hanTET1);
    WSContentSend_PD("{s}Energy T2 Ponta {m} %3_f kWh{e}",
                     &hanTET2);
    WSContentSend_PD(
        "{s}Energy T3 Cheias {m} %3_f kWh{e}", &hanTET3);

    WSContentSend_PD("{s}<br>{m} {e}");

    WSContentSend_PD(
        "{s}Energy Total Import {m} %3_f kWh{e}",
        &hanTEI);
    WSContentSend_PD(
        "{s}Energy Total Export {m} %3_f kWh{e}",
        &hanTEE);

    WSContentSend_PD("{s}<br>{m} {e}");

    if (hanEB == 3) {
      WSContentSend_PD(
          "{s}Energy Import L1 {m} %3_f kWh{e}", &hTEIL1);
      WSContentSend_PD("{s}L2 {m} %3_f kWh{e}", &hTEIL2);
      WSContentSend_PD("{s}L3 {m} %3_f kWh{e}", &hTEIL3);

      WSContentSend_PD("{s}<br>{m} {e}");
    }

    char hLPDate[15];
    sprintf(hLPDate, "%04d-%02d-%02d", hLP1YY, hLP1MT,
            hLP1DD);

    WSContentSend_PD("{s}LP1 Date {m} %s{e}", hLPDate);

    char hLPClock[15];
    sprintf(hLPClock, "%02d:%02d Z%s", hLP1HH, hLP1MM,
            hLP1gmt);

    WSContentSend_PD("{s}LP1 Time {m} %s{e}", hLPClock);

    WSContentSend_PD("{s}LP2 AMR {m} %d{e}", hLP2);

    WSContentSend_PD("{s}LP3 Import Inc  {m} %3_f kWh{e}",
                     &hLP3);
    WSContentSend_PD("{s}LP4 {m} %3_f kWh{e}", &hLP4);
    WSContentSend_PD("{s}LP5 {m} %3_f kWh{e}", &hLP5);
    WSContentSend_PD("{s}LP6 Export Inc {m} %3_f kWh{e}",
                     &hLP6);

    if (hLPid[6] != 10) {
      WSContentSend_PD(
          "{s}Error: Export Inc {m} %d != 10 {e}",
          hLPid[6]);
    }

    WSContentSend_PD("{s}LP7 {m} %3_f kWh{e}", &hLP7);
    WSContentSend_PD("{s}LP8 {m} %3_f kWh{e}", &hLP8);

    char tmpLPid[25];
    sprintf(tmpLPid, "%d %d %d %d %d %d %d %d", hLPid[1],
            hLPid[2], hLPid[3], hLPid[4], hLPid[5],
            hLPid[6], hLPid[7], hLPid[8]);

    WSContentSend_PD("{s}IDs: %s {m} {e}", tmpLPid);

    WSContentSend_PD("{s}<br>{m} {e}");

    WSContentSend_PD("{s}Contract T1 {m} %2_f kVA{e}",
                     &hCT1);

    char tarifa[10];

    switch (hTariff) {
      case 1:
        sprintf(tarifa, "%s", "Vazio");
        break;
      case 2:
        sprintf(tarifa, "%s", "Ponta");
        break;
      case 3:
        sprintf(tarifa, "%s", "Cheias");
        break;
      default:
        sprintf(tarifa, "Error %d", hTariff);
    }

    WSContentSend_PD("{s}Tariff {m} %s{e}", tarifa);

    WSContentSend_PD("{s}<br>{m} {e}");

    char _emi[20];

    switch (hMnfC) {
      case 6623491:
        sprintf(_emi, "%s", "T Janz GPRS");
        break;
      case 6754306:
        sprintf(_emi, "%s", "T Landis+Gyr S3");
        break;
      case 6754307:
        sprintf(_emi, "%s", "T Landis+Gyr S5");
        break;
      case 11014146:
        sprintf(_emi, "%s", "T Sagem CX2000-9");
        break;
      case 18481154:
        sprintf(_emi, "%s", "M Kaifa MA109P");
        break;
      default:
        sprintf(_emi, "%s", "???");
    }

    WSContentSend_PD("{s}EMI Manufacturer Year {m} %d{e}",
                     hMnfY);

    WSContentSend_PD("{s}EMI ( %s ) {m} %d{e}", _emi,
                     hMnfC);

    WSContentSend_PD("{s}<br>{m} {e}");

    WSContentSend_PD("{s}EMI Firmware: {m} {e}");
    WSContentSend_PD("{s}1. Core {m} %s{e}", hFw1);
    WSContentSend_PD("{s}2. App {m} %s{e}", hFw2);
    WSContentSend_PD("{s}3. Com {m} %s{e}", hFw3);

    WSContentSend_PD("{s}<br>{m} {e}");

  }  // eof !json

}  // HanJson

// ********************************************
// ********************************************

const char HanCommands[] PROGMEM =
    "|"  // No Prefix
    "HanDelay|"
    "HanDelayWait|"
    "HanDelayError|"
    "HanProfile";

void (*const HanCommand[])(void) PROGMEM = {
    &CmdHanDelay, &CmdHanDelayWait, &CmdHanDelayError,
    &CmdHanProfile};

//

void CmdHanProfile(void) {
  hanWork = false;
  hanDelay = 15000;

  uint8_t hRes;

  if (hLPX[0] == 0) {
    //
    node.clearTransmitBuffer();
    delay(100);
    node.clearResponseBuffer();
    delay(100);
    //
    hRes = node.readInputRegisters(0x0082, 2);
    if (hRes == node.ku8MBSuccess) {
      hLPX[0] = (node.getResponseBuffer(1) |
                 node.getResponseBuffer(0) << 16);
      hLPX[1] = (node.getResponseBuffer(3) |
                 node.getResponseBuffer(2) << 16);
      hanBlink();
    }
  }

  uint16_t hLPX1YY = 0;
  uint8_t hLPX1MT = 0;
  uint8_t hLPX1DD = 0;
  uint8_t hLPX1HH = 0;
  uint8_t hLPX1MM = 0;
  uint8_t hLPX1gmt = 99;

  uint16_t hLPX2 = 0;  // tweaked to 16bits

  uint32_t hLPX3 = 0;
  uint32_t hLPX4 = 0;
  uint32_t hLPX5 = 0;
  uint32_t hLPX6 = 0;
  uint32_t hLPX7 = 0;
  uint32_t hLPX8 = 0;

  uint16_t getLP = 0;
  char resX[50];

  if ((XdrvMailbox.payload >= 1) &&
      (XdrvMailbox.payload <= hLPX[0])) {
    // *****
    getLP = XdrvMailbox.payload;

    node.clearTransmitBuffer();
    delay(100);
    node.clearResponseBuffer();
    delay(100);

    node.setTimeout(2000);

    hRes = node.readProfileX(getLP, 0x00);
    if (hRes == node.ku8MBSuccess) {
      hLPX1YY = node.getResponseBuffer(0);
      hLPX1MT = node.getResponseBuffer(1) >> 8;
      hLPX1DD = node.getResponseBuffer(1) & 0xFF;

      hLPX1HH = node.getResponseBuffer(2) & 0xFF;
      hLPX1MM = node.getResponseBuffer(3) >> 8;

      hLPX1gmt = node.getResponseBuffer(5) & 0xFF;

      // tweaked to 16bits. branch: LP1.
      hLPX2 = node.getResponseBuffer(6);

      hLPX3 = (node.getResponseBuffer(8) |
               node.getResponseBuffer(7) << 16);
      hLPX4 = (node.getResponseBuffer(10) |
               node.getResponseBuffer(9) << 16);
      hLPX5 = (node.getResponseBuffer(12) |
               node.getResponseBuffer(11) << 16);
      hLPX6 = (node.getResponseBuffer(14) |
               node.getResponseBuffer(13) << 16);
      hLPX7 = (node.getResponseBuffer(16) |
               node.getResponseBuffer(15) << 16);
      hLPX8 = (node.getResponseBuffer(18) |
               node.getResponseBuffer(17) << 16);
      hanBlink();

      char tmpGMT[5];

      if (hLPX1gmt == 0x80) {
        sprintf(tmpGMT, "01");
      } else {
        sprintf(tmpGMT, "00");
      }

      sprintf(resX,
              "%04d,%04d-%02d-%02dT%02d:%02dZ%s,"
              "%02d,%04d,%04d,%04d,%04d",
              getLP, hLPX1YY, hLPX1MT, hLPX1DD, hLPX1HH,
              hLPX1MM, tmpGMT, hLPX2, hLPX3, hLPX4, hLPX5,
              hLPX6);

    } else {
      sprintf(resX, "%04d,Error,Code,%d", getLP, hRes);
    }
    // *****
  } else {
    sprintf(resX, "%04d,Error,Limit,%04d,%04d", getLP,
            hLPX[0], hLPX[1]);
  }

  hanRead = millis();
  hanWork = false;
  //
  sprintf(hStatus, "Cmd");
  ResponseCmndChar(resX);
}

//

void CmdHanDelay(void) {
  if ((XdrvMailbox.payload >= 0) &&
      (XdrvMailbox.payload <= 900)) {
    hanDelay = XdrvMailbox.payload * 1000;
  } else {
    hanDelay = 300000;
  }
  AddLog(LOG_LEVEL_INFO, PSTR("HanDelay: Paused %ds"),
         hanDelay / 1000);
  sprintf(hStatus, "Cmd");
  ResponseCmndDone();
}

//

void CmdHanDelayWait(void) {
  if ((XdrvMailbox.payload >= 100) &&
      (XdrvMailbox.payload <= 99000)) {
    hanDelayWait = XdrvMailbox.payload;
  }
  AddLog(LOG_LEVEL_INFO, PSTR("HanDelayWait: %dms"),
         hanDelayWait);
  ResponseCmndDone();
}

//

void CmdHanDelayError(void) {
  if ((XdrvMailbox.payload >= 1000) &&
      (XdrvMailbox.payload <= 900000)) {
    hanDelayError = XdrvMailbox.payload;
  }
  AddLog(LOG_LEVEL_INFO, PSTR("HanDelayError: %dms"),
         hanDelayError);
  ResponseCmndDone();
}

// main tasmota function

bool Xdrv100(uint32_t function) {
  bool result = false;

  if (FUNC_INIT == function) {
    HanInit();
    AddLog(LOG_LEVEL_INFO, PSTR("HAN: Done !"));
  } else if (hDrvInit) {
    switch (function) {
      case FUNC_LOOP:
        HanDoWork();
        break;
      case FUNC_JSON_APPEND:
        HanJson(true);
        break;
      case FUNC_COMMAND:
        result = DecodeCommand(HanCommands, HanCommand);
        break;
#ifdef USE_WEBSERVER
      case FUNC_WEB_SENSOR:
        HanJson(false);
        break;
#endif  // USE_WEBSERVER
    }
  }

  return result;
}

#warning **** HAN_V2 End! ****

#endif  // USE_HAN_V2
