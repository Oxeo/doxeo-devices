#include <SPI.h>
#include <Mirf.h>
#include <nRF24L01.h>
#include <MirfHardwareSpiDriver.h>
#include <LowPower.h>
#include <DoxeoConfig.h>

#define WAKE_UP_PIN 2

// Nodes
char* nodes[] = {DOXEO_ADDR_MOTHER, DOXEO_ADDR_SOUND};
const int nbNodes = 2;
int selectedNode = 0;

byte data[32];

void setup() {
  randomSeed(analogRead(0));//initialise la séquence aléatoir

  pinMode(WAKE_UP_PIN, INPUT);

  // Configure NRF communication
  Mirf.cePin = 9; // Broche CE sur D9
  Mirf.csnPin = 10; // Broche CSN sur D10
  Mirf.spi = &MirfHardwareSpi;
  Mirf.init();
  Mirf.channel = DOXEO_CHANNEL; // Choix du canal de communication (128 canaux disponibles, de 0 à 127)
  Mirf.payload = 32; // Taille d'un data (maximum 32 octets)
  Mirf.config(); // Sauvegarde la configuration dans le module radio
  Mirf.configRegister(RF_SETUP, 0x26); // sortie 0dBm @ 250Kbs to improve distance
  Mirf.configRegister(SETUP_RETR, 0x3F);  // send retry 15 times
  Mirf.setTADDR((byte *) DOXEO_ADDR_MOTHER);

  sendMessage("init done");
}

void loop() {
  delay(500);

  String status = "close";
  if (digitalRead(WAKE_UP_PIN) == HIGH) {
    status = "open";
  }

  sendMessage(status);
  sleepForever();
}

void sendMessage(String msg) {
  bool success;

  for (int i = 0; i < 5; ++i) {
    success = sendNrf(String(DOXEO_ADDR_DOOR1) + ';' + String(DOXEO_ADDR_MOTHER) + ";" + msg);

    if (success) {
      break;
    } else {
      delay(random(100)); // avoid colision
    }
  }
}

bool sendNrf(String message) {
  message.getBytes(data, 32);

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

  Mirf.powerDown(); // power down NRF to save energy
  return Mirf.sendWithSuccess;
}

void sleepForever() {
  attachInterrupt(digitalPinToInterrupt(WAKE_UP_PIN), wakeUp, CHANGE);
  LowPower.powerDown(SLEEP_FOREVER, ADC_OFF, BOD_OFF);
  detachInterrupt(digitalPinToInterrupt(WAKE_UP_PIN));
}

void wakeUp()
{
  // do nothing
}
