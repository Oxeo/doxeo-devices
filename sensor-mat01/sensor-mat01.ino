#include <SPI.h>      // Pour la communication via le port SPI
#include <Mirf.h>     // Pour la gestion de la communication
#include <nRF24L01.h> // Pour les définitions des registres du nRF24L01
#include <MirfHardwareSpiDriver.h> // Pour la communication SPI (ne cherchez pas à comprendre)
#include <LowPower.h>
#include <DoxeoConfig.h>

#define PIN_WAKEUP 2
#define PIN_ALIM 4

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

byte data[32];

void setup() {
  pinMode(PIN_WAKEUP, INPUT);
  pinMode(PIN_ALIM, OUTPUT);
  
  // Configure NRF communication
  Mirf.cePin = 9; // Broche CE sur D9
  Mirf.csnPin = 10; // Broche CSN sur D10
  Mirf.spi = &MirfHardwareSpi; // On veut utiliser le port SPI hardware
  Mirf.init(); // Initialise la bibliothèque
  Mirf.channel = 1; // Choix du canal de communication (128 canaux disponibles, de 0 à 127)
  Mirf.payload = 32; // Taille d'un data (maximum 32 octets)
  Mirf.config(); // Sauvegarde la configuration dans le module radio
  Mirf.configRegister(RF_SETUP, 0x26); // sortie 0dBm @ 250Kbs to improve distance
  Mirf.setTADDR((byte *) DOXEO_ADDR_MOTHER); // Adresse de transmission
  Mirf.setRADDR((byte *) DOXEO_ADDR_MAT01); // Adresse de réception

  digitalWrite(PIN_ALIM, HIGH);

  sendStatus("init");
}

void wakeUp()
{

}

void loop() {
  // Allow wake up pin to trigger interrupt on change.
  attachInterrupt(digitalPinToInterrupt(PIN_WAKEUP), wakeUp, CHANGE);
  LowPower.powerDown(SLEEP_FOREVER, ADC_OFF, BOD_OFF);
  detachInterrupt(digitalPinToInterrupt(PIN_WAKEUP));

  if (digitalRead(PIN_WAKEUP) == HIGH) {
    digitalWrite(PIN_ALIM, LOW);
    sendStatus("on");

    do {
      digitalWrite(PIN_ALIM, LOW);
      LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);
      digitalWrite(PIN_ALIM, HIGH);
      LowPower.powerDown(SLEEP_30MS, ADC_OFF, BOD_OFF);
    } while (digitalRead(PIN_WAKEUP) == HIGH);
  
    sendStatus("off");
  }
}

void sendStatus(String state) {
  String message = "doormat;" + state;
  message.getBytes(data, 32);

  // Configure NRF communication
  Mirf.cePin = 9; // Broche CE sur D9
  Mirf.csnPin = 10; // Broche CSN sur D10
  Mirf.spi = &MirfHardwareSpi; // On veut utiliser le port SPI hardware
  Mirf.init(); // Initialise la bibliothèque
  Mirf.channel = 1; // Choix du canal de communication (128 canaux disponibles, de 0 à 127)
  Mirf.payload = 32; // Taille d'un data (maximum 32 octets)
  Mirf.config(); // Sauvegarde la configuration dans le module radio
  Mirf.configRegister(RF_SETUP, 0x26); // sortie 0dBm @ 250Kbs to improve distance
  Mirf.setTADDR((byte *) DOXEO_ADDR_MOTHER); // Adresse de transmission
  Mirf.setRADDR((byte *) DOXEO_ADDR_MAT01); // Adresse de réception

  for (int i=0; i<5; ++i) {
    Mirf.send(data);
    while (Mirf.isSending());
    LowPower.powerDown(SLEEP_1S, ADC_OFF, BOD_OFF);
  }

  // power down NRF to save energy
  Mirf.powerDown();
}

