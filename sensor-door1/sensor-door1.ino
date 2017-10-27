#include <SPI.h>      // Pour la communication via le port SPI
#include <Mirf.h>     // Pour la gestion de la communication
#include <nRF24L01.h> // Pour les définitions des registres du nRF24L01
#include <MirfHardwareSpiDriver.h> // Pour la communication SPI (ne cherchez pas à comprendre)
#include <LowPower.h>
#include <DoxeoConfig.h>

 /* 
  *  Pins:
  * Hardware SPI:
  * MISO -> 12
  * MOSI -> 11
  * SCK -> 13
  *
  * Configurable:
  * CE -> 9
  * CSN -> 10
  */

const int wakeUpPin = 2;
int cpt = 0;

void setup() {
  // Configure NRF communication
  Mirf.cePin = 9; // Broche CE sur D9
  Mirf.csnPin = 10; // Broche CSN sur D10
  Mirf.spi = &MirfHardwareSpi; // On veut utiliser le port SPI hardware
  Mirf.init(); // Initialise la bibliothèque
  Mirf.channel = DOXEO_CHANNEL; // Choix du canal de communication (128 canaux disponibles, de 0 à 127)
  Mirf.payload = 32; // Taille d'un data (maximum 32 octets)
  Mirf.config(); // Sauvegarde la configuration dans le module radio
  Mirf.configRegister(RF_SETUP, 0x26); // sortie 0dBm @ 250Kbs to improve distance
  Mirf.setTADDR((byte *) DOXEO_ADDR_MOTHER); // Adresse de transmission
  Mirf.setRADDR((byte *) DOXEO_ADDR_DOOR1); // Adresse de réception

  // Configure wake up pin as input.
  // This will consumes few uA of current.
  pinMode(wakeUpPin, INPUT);

  // Send init data
  byte data[32];
  String message = "door1;init";
  message.getBytes(data, 32);
  Mirf.send(data); // On envoie le data
  while (Mirf.isSending());
  cpt = 0;
}

void wakeUp()
{
  cpt = 0;
}

void loop() {
  // Allow wake up pin to trigger interrupt on change.
  attachInterrupt(digitalPinToInterrupt(wakeUpPin), wakeUp, CHANGE);

  // Enter power down state with ADC and BOD module disabled.
  // Wake up when wake up pin is low.
  if (cpt >= 5) {
    LowPower.powerDown(SLEEP_FOREVER, ADC_OFF, BOD_OFF);
  } else {
    LowPower.powerDown(SLEEP_1S, ADC_OFF, BOD_OFF);
  }

  // Disable external pin interrupt on wake up pin.
  detachInterrupt(digitalPinToInterrupt(wakeUpPin));

  // Send data
  byte data[32];
  String message = "door1;";

  if (digitalRead(wakeUpPin) == HIGH) {
    message += "open";
  } else {
    message += "close";
  }

  message.getBytes(data, 32);
  Mirf.send(data); // On envoie le data
  while (Mirf.isSending());

  cpt++;

  // power down NRF to save energy
  Mirf.powerDown();
}

