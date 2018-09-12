#include <SPI.h>
#include <Mirf.h>
#include <nRF24L01.h>
#include <MirfHardwareSpiDriver.h>
#include <LowPower.h>
#include <DoxeoConfig.h>

//#define DEBUG
#include "DebugUtils.h"

#define NRF_INTERRUPT 2
#define KEY_1 3
#define KEY_2 4
#define BUZZER 7
#define SIREN 8
#define BATTERY_SENSE A0

char* nodes[] = {DOXEO_ADDR_MOTHER, DOXEO_ADDR_SOUND};
const int nbNodes = 2;
int selectedNode = 0;
byte data[32];

// Token ID
unsigned long tokenId = 0;
unsigned long tokenIdTime = 0;

// Battery sense
unsigned long batteryComputeCpt = 0;
int batteryPcnt = 0;
int oldBatteryPcnt = 0;
float batteryV = 0.0;

// wake up time
unsigned long lastnrfInterruptTime = 0;
bool checkNewMsg = false;

// Status
unsigned long interruptKeyTime = 0;
unsigned long sirenTime = 0;
char sirenState = 0;
bool sirenTest = false;
char bipNumber = 6;

void setup() {
  // use the 1.1 V internal reference for battery sens
  analogReference(INTERNAL);

  // init PIN
  pinMode(NRF_INTERRUPT, INPUT);
  pinMode(KEY_1, INPUT);
  pinMode(KEY_2, INPUT);
  pinMode(BUZZER, OUTPUT);
  pinMode(SIREN, OUTPUT);
  digitalWrite(BUZZER, LOW);
  digitalWrite(SIREN, LOW);

  // init serial for debugging
#ifdef DEBUG
  Serial.begin(9600);
  Serial.println("debug mode enable");
#endif

  // init NRF
  Mirf.cePin = 9;
  Mirf.csnPin = 10;
  Mirf.spi = &MirfHardwareSpi; // Hardware SPI: MISO -> 12, MOSI -> 11, SCK -> 13
  Mirf.init();
  Mirf.channel = DOXEO_CHANNEL; // Choix du canal de communication (128 canaux disponibles, de 0 à 127)
  Mirf.setRADDR((byte *) DOXEO_ADDR_SIREN);
  Mirf.payload = 32; // Taille d'un data (maximum 32 octets)
  Mirf.config(); // Sauvegarde la configuration dans le module radio
  Mirf.configRegister(RF_SETUP, 0x26); // sortie 0dBm @ 250Kbs to improve distance
  Mirf.configRegister(EN_RXADDR, 0x02); // only pipe 1 can received
  Mirf.configRegister(SETUP_RETR, 0x3F);  // retry 15x
  Mirf.setTADDR((byte *) nodes[0]);

  // init NRF interrupt
  if (digitalRead(NRF_INTERRUPT) == LOW) {
    Mirf.configRegister(STATUS, 0x70); // clear IRQ register
  }
  attachInterrupt(digitalPinToInterrupt(NRF_INTERRUPT), nrfInterrupt, FALLING);

  // init key interrupt
  attachInterrupt(digitalPinToInterrupt(KEY_1), keyInterrupt, FALLING);

  batteryComputeCpt = 0;
  batteryPcnt = 0;
  oldBatteryPcnt = 0;
  batteryV = 0.0;

  selectedNode = 0;
  sendMessage("init done");
}


void loop() {
  // message received
  if (checkNewMsg && Mirf.dataReady()) {
    // read message
    byte byteMessage[32];
    Mirf.getData(byteMessage);
    String msg = String((char*) byteMessage);
    DEBUG_PRINT("message received:" + msg);

    // parse message {receptorName};{id};{message}
    String from = parseMsg(msg, ';', 0);
    String receptorName = parseMsg(msg, ';', 1);
    int id = parseMsg(msg, ';', 2).toInt();
    String data = parseMsg(msg, ';', 3);
    String message = parseMsg(data, '-', 0);
    String bipToDo = parseMsg(data, '-', 1);

    // handle message
    if (receptorName != DOXEO_ADDR_SIREN) {
      // do nothing, the message is not for us
    } else if (id == tokenId && (millis() - tokenIdTime < 60000)) {
      // already done, send success in case the previous message was not received
      sendAck(id);
    } else if (id == 0) {
      sendMessage("missing ID");
    } else if (message == "ping") {
      sendAck(id);
    } else if (message == "start") { // start siren
      sendAck(id);
      sirenTest = false;
      bipNumber = bipToDo.toInt();
      enableSiren(true);
      sendMessage("started");
    } else if (message == "start_test") { // start siren
      sendAck(id);
      sirenTest = true;
      bipNumber = bipToDo.toInt();
      enableSiren(true);
      sendMessage("test started");
    } else if (message == "stop") { // stop siren
      sendAck(id);
      if (isSirenOn()) {
        enableSiren(false);
        sendMessage("stopped by user");
      }
    } else if (message == "battery") { // send battery level
      sendAck(id);
      computeBatteryLevel();
      sendBatteryLevel();
    } else {
      sendAck(id);
      sendMessage("args error");
    }
  } else if (millis() - interruptKeyTime < 5000 || isSirenOn()) {  // key pressed or siren on
    manageSiren();

    if (successCode()) {
      if (isSirenOn()) {
        enableSiren(false);
        sendMessage("stopped by key");
      } else {
#ifdef DEBUG
        sirenTest = true;
#endif
        sound(true);
        delayMicroseconds(100);
        sound(false);
        sendMessage("code entered");
      }
    }
  } else if (batteryComputeCpt == 10000) { // Check battery level every 6 hours
    batteryComputeCpt = 0;
    computeBatteryLevel();

    if (abs(oldBatteryPcnt - batteryPcnt) >= 1) {
      sendBatteryLevel();
      oldBatteryPcnt = batteryPcnt;
    }
  } else {
    checkNewMsg = false;
    batteryComputeCpt++;

    // Sleep 2S
    Mirf.powerDown();
#ifdef DEBUG
    delay(2000);
#else
    sleep(SLEEP_2S);
#endif

    // enable reception during 15ms
    Mirf.powerUpRx();
#ifdef DEBUG
    delay(15);
#else
    sleep(SLEEP_15MS);
#endif
  }
}

bool successCode() {
  static unsigned long btTime = 0;
  static bool key1Pressed = false;
  static bool key2Pressed = false;
  static char state = 0;
  bool success = false;

  if (millis() - btTime >= 5000) {
    state = 0;
  }

  // manage key1 press
  if (!key1Pressed && digitalRead(KEY_1) == LOW && (millis() - btTime >= 200)) {
    DEBUG_PRINT("key1 pressed");

    btTime = millis();
    key1Pressed = true;
  }
  if (key1Pressed && digitalRead(KEY_1) == HIGH && (millis() - btTime >= 200)) {
    DEBUG_PRINT("key1 released");
    key1Pressed = false;
  }

  // manage key2 press
  if (!key2Pressed && digitalRead(KEY_2) == LOW && (millis() - btTime >= 200)) {
    DEBUG_PRINT("key2 pressed");

    btTime = millis();
    key2Pressed = true;
  }
  if (key2Pressed && digitalRead(KEY_2) == HIGH && (millis() - btTime >= 200)) {
    DEBUG_PRINT("key2 released");
    key2Pressed = false;
  }

  // manage code 121
  if (state == 0 && key1Pressed) {
    state++;
  } else if (state == 1 && !key1Pressed) {
    state++;
  } else if (state == 2) {
    if (key2Pressed) {
      state++;
    } else if (key1Pressed) {
      state = 1;
    }
  } else if (state == 3 && !key2Pressed) {
    state++;
  } else if (state == 4) {
    if (key1Pressed) {
      state++;
    } else if (key2Pressed) {
      state = 0;
    }
  } else if (state == 5 && !key1Pressed) {
    DEBUG_PRINT("success code");
    success = true;
    state = 0;
  }

  return success;
}

void manageSiren() {
  if (sirenState == 0) {
    // nothing to do
  } else if (sirenState == 1) {
    sound(true);
    sirenTime = millis();
    sirenState++;
  } else if (sirenState == 0 && millis() - sirenTime >= 5) {
    sound(false);
    sirenTime = millis();
    sirenState++;
  } else if ((sirenState % 2 != 0 && sirenState < bipNumber * 2 + 2) && millis() - sirenTime >= 1000) {
    sound(true);
    sirenTime = millis();
    sirenState++;
  } else if ((sirenState % 2 == 0 && sirenState < bipNumber * 2 + 2) && millis() - sirenTime >= 5) {
    sound(false);
    sirenTime = millis();
    sirenState++;
  } else if (sirenState == bipNumber * 2 && millis() - sirenTime >= 300000) {
    enableSiren(false);
    sendMessage("stopped");
  }
}

void enableSiren(bool enable) {
  if (enable) {
    sirenState = 1;
  } else {
    sirenState = 0;
    sound(false);
    sirenTest = false;
  }
}

bool isSirenOn() {
  return sirenState != 0;
}

void sound(bool enable) {
  if (enable) {
    if (sirenTest) {
      tone(BUZZER, 1000); // Send 1KHz sound signal...
    } else {
      digitalWrite(SIREN, HIGH);
    }
  } else {
    if (sirenTest) {
      noTone(BUZZER);
    } else {
      digitalWrite(SIREN, LOW);
    }
  }
}

int computeBatteryLevel() {
  int sensorValue = analogRead(BATTERY_SENSE);
  batteryPcnt = sensorValue / 10;

  // ((2M+1M)/1M)*1.1 = Vmax = 3.3 Volts
  // 3.3/1023 = Volts per bit = 0.003225806
  batteryV  = sensorValue * 0.003225806;
}

void sendBatteryLevel() {
  sendMessage("battery=" + String(batteryV) + "v" + String(batteryPcnt) + "%");
}

void sendAck(int id) {
  sendMessage(String(id) + ";success");
  tokenId = id;
  tokenIdTime = millis();
}

void sendMessage(String msg) {
  bool success;

  for (int i = 0; i < 3; ++i) {
    success = sendNrf(String(DOXEO_ADDR_SIREN) + ';' + String(DOXEO_ADDR_MOTHER) + ";" + msg);

    if (success) {
      break;
    } else {
      delay(100);
    }
  }
}

bool sendNrf(String message) {
  DEBUG_PRINT("send message: " + message);
  message.getBytes(data, 32);
  Mirf.configRegister(EN_RXADDR, 0x03); // only pipe 0 and 1 can received for ACK

  for (unsigned char i = 0; i < nbNodes; ++i) {
    Mirf.send(data);
    while (Mirf.isSending());

    if (Mirf.sendWithSuccess == true) {
      break;
    } else {
      // change selected node
      selectedNode = (selectedNode + 1 < nbNodes) ? selectedNode + 1 : 0;
      Mirf.setTADDR((byte *) nodes[selectedNode]);
    }
  }

  Mirf.configRegister(EN_RXADDR, 0x02); // only pipe 1 can received
  return Mirf.sendWithSuccess;
}

void sleep(period_t period) {
  if (digitalRead(NRF_INTERRUPT) == LOW) {
    Mirf.configRegister(STATUS, 0x70); // clear IRQ register
  }

  LowPower.powerDown(period, ADC_OFF, BOD_OFF);
}

String parseMsg(String data, char separator, int index)
{
  int found = 0;
  int strIndex[] = {0, -1};
  int maxIndex = data.length() - 1;

  for (int i = 0; i <= maxIndex && found <= index; i++) {
    if (data.charAt(i) == separator || i == maxIndex) {
      found++;
      strIndex[0] = strIndex[1] + 1;
      strIndex[1] = (i == maxIndex) ? i + 1 : i;
    }
  }

  return found > index ? data.substring(strIndex[0], strIndex[1]) : "";
}

void nrfInterrupt()
{
  checkNewMsg = true;
}

void keyInterrupt()
{
  interruptKeyTime = millis();
}
