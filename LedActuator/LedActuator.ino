#include <SPI.h>
#include <Mirf.h>
#include <nRF24L01.h>
#include <MirfHardwareSpiDriver.h>
#include <LowPower.h>
#include <DoxeoConfig.h>

//#define DEBUG
#include "DebugUtils.h"

#define NRF_INTERRUPT 2
#define LED 3
#define BATTERY_SENSE A0

char* nodes[] = {DOXEO_ADDR_MOTHER};
const int nbNodes = 1;
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
bool checkNewMsg = false;

// Status
unsigned long ledTime = 0;
char ledState = 0;
unsigned int ledTimeOn = 0;

void setup() {
  // use the 1.1 V internal reference for battery sens
  analogReference(INTERNAL);

  // init PIN
  pinMode(NRF_INTERRUPT, INPUT);
  pinMode(LED, OUTPUT);
  digitalWrite(LED, LOW);

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
  Mirf.setRADDR((byte *) DOXEO_ADDR_LED1);
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
    String timePowerOn = parseMsg(data, '-', 1);

    // handle message
    if (receptorName != DOXEO_ADDR_LED1) {
      // do nothing, the message is not for us
    } else if (id == tokenId && (millis() - tokenIdTime < 60000)) {
      // already done, send success in case the previous message was not received
      sendAck(id);
    } else if (id == 0) {
      sendMessage("missing ID");
    } else if (message == "ping") {
      sendAck(id);
    } else if (message == "start") { // power on led
      sendAck(id);
      ledTimeOn = timePowerOn.toInt() * 30;
      enableLed(true);
      sendMessage("started");
      delay(1000);
    } else if (message == "stop") { // power down
      sendAck(id);
      if (isLedOn()) {
        enableLed(false);
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
  } else if (isLedOn()) {  // led on
    if (ledTime >= ledTimeOn) {
      enableLed(false);
      sendMessage("stopped");
    }
  } else if (batteryComputeCpt == 10000) { // Check battery level every 6 hours
    batteryComputeCpt = 0;
    computeBatteryLevel();

    if (abs(oldBatteryPcnt - batteryPcnt) >= 1) {
      sendBatteryLevel();
      oldBatteryPcnt = batteryPcnt;
    }
  }

  checkNewMsg = false;
  batteryComputeCpt++;
  ledTime++;

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

void enableLed(bool enable) {
  if (enable) {
    ledState = 1;
    ledTime = 0;
    digitalWrite(LED, HIGH);
  } else {
    ledState = 0;
    digitalWrite(LED, LOW);
  }
}

bool isLedOn() {
  return ledState != 0;
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
    success = sendNrf(String(DOXEO_ADDR_LED1) + ';' + String(DOXEO_ADDR_MOTHER) + ";" + msg);

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

