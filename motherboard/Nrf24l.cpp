#include "Nrf24l.h"

#include <SPI.h>      // Pour la communication via le port SPI
#include <Mirf.h>     // Pour la gestion de la communication
#include <nRF24L01.h> // Pour les définitions des registres du nRF24L01
#include <MirfHardwareSpiDriver.h> // Pour la communication SPI (ne cherchez pas à comprendre)
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

void MyNrf24l::init() {
  Mirf.cePin = 9; // Broche CE sur D9
  Mirf.csnPin = 10; // Broche CSN sur D10
  Mirf.spi = &MirfHardwareSpi; // On veut utiliser le port SPI hardware
  Mirf.init(); // Initialise la bibliothèque

  Mirf.channel = DOXEO_CHANNEL; // Choix du canal de communication (128 canaux disponibles, de 0 à 127)
  Mirf.payload = 32; // Taille d'un message (maximum 32 octets)
  Mirf.config(); // Sauvegarde la configuration dans le module radio

  Mirf.configRegister(RF_SETUP, 0x26); // to send much longeur

  Mirf.setRADDR((byte *) DOXEO_ADDR_MOTHER); // Adresse de réception
}

