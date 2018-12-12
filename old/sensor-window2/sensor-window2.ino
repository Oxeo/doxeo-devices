#include <SPI.h>
#include <Mirf.h>
#include <nRF24L01.h>
#include <MirfHardwareSpiDriver.h>
#include <LowPower.h>
#include <DoxeoConfig.h>

//#define DEBUG
#include "DebugUtils.h"

#define NRF_ADDR "door2"

#define DOOR 2

enum Status {open, close, unknown};

char* nodes[] = {DOXEO_ADDR_MOTHER, DOXEO_ADDR_SOUND};
const int nbNodes = 2;
int selectedNode = 0;
byte data[32];

// Status
Status doorStatus = unknown;

void setup() {
  // init PIN
  pinMode(DOOR, INPUT);
  pinMode(A0, INPUT);

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
  Mirf.setRADDR((byte *) NRF_ADDR);
  Mirf.payload = 32; // Taille d'un data (maximum 32 octets)
  Mirf.config(); // Sauvegarde la configuration dans le module radio
  Mirf.configRegister(RF_SETUP, 0x26); // sortie 0dBm @ 250Kbs to improve distance
  Mirf.configRegister(EN_RXADDR, 0x02); // only pipe 1 can received
  Mirf.configRegister(SETUP_RETR, 0x3F);  // retry 15x
  Mirf.setTADDR((byte *) nodes[0]);

  // status
  doorStatus = unknown;

  selectedNode = 0;
  sendMessage("", "init done");
}


void loop() {
 pinMode(DOOR, INPUT_PULLUP);

  if (digitalRead(DOOR) == HIGH && doorStatus != open) {
    doorStatus = open;
    sendMessage("", "open");
  } else if (digitalRead(DOOR) == LOW && doorStatus != close) {
    doorStatus = close;
    sendMessage("", "close");
  }

  pinMode(DOOR, INPUT);
  LowPower.powerDown(SLEEP_2S, ADC_OFF, BOD_OFF);
}

void sendMessage(String device, String msg) {
  bool success;

  for (int i = 0; i < 3; ++i) {

    if (device == "") {
      success = sendNrf(String(NRF_ADDR) + ';' + String(DOXEO_ADDR_MOTHER) + ";" + msg);
    } else {
      success = sendNrf(String(NRF_ADDR) + '-' + device + ';' + String(DOXEO_ADDR_MOTHER) + ';' + msg);
    }

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

void doorInterruptHandle()
{
  // nothing to do
}
