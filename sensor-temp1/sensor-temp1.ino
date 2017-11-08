#include <SPI.h>      // Pour la communication via le port SPI
#include <Mirf.h>     // Pour la gestion de la communication
#include <nRF24L01.h> // Pour les définitions des registres du nRF24L01
#include <MirfHardwareSpiDriver.h> // Pour la communication SPI (ne cherchez pas à comprendre)

#include <OneWire.h>           // Temperature sensor
#include <DallasTemperature.h> // Temperature sensor

#include <LowPower.h>
#include <DoxeoConfig.h>

#define PIN_TEMPERATURE 4
#define PIN_ALIM_TEMPERATURE 5
#define TEMPERATURE_PRECISION 11 //Max 12 bit, min 9 bit

// Temperature sensor
OneWire oneWire(PIN_TEMPERATURE);
DallasTemperature sensors(&oneWire);

/*
    Pins:
   Hardware SPI:
   MISO -> 12
   MOSI -> 11
   SCK -> 13

   Configurable:
   CE -> 9
   CSN -> 10
*/

int cpt = 0;
byte data[32];
byte tempAddress[8];

void setup() { 
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

  // Send init data
  String message = "temperature1;init";
  message.getBytes(data, 32);
  Mirf.send(data);
  while (Mirf.isSending());
  Mirf.powerDown();

  // configure temperature sensor
  pinMode(PIN_ALIM_TEMPERATURE, OUTPUT);
  digitalWrite(PIN_ALIM_TEMPERATURE, HIGH);
  delay(50); // Allow 50ms for the sensor to be ready
  sensors.begin(); 
  sensors.setWaitForConversion(false);   
  int numSensors=sensors.getDeviceCount();
  oneWire.search(tempAddress);
  sensors.setResolution(tempAddress, TEMPERATURE_PRECISION);

  cpt = 0;
}

void loop() {
  if (cpt % 75 == 0) {
    // take temperature
    pinMode(PIN_ALIM_TEMPERATURE, OUTPUT); // set power pin for DS18B20 to output  
    digitalWrite(PIN_ALIM_TEMPERATURE, HIGH);
    LowPower.powerDown(SLEEP_30MS, ADC_OFF, BOD_OFF);
    sensors.setResolution(tempAddress, TEMPERATURE_PRECISION); 
    sensors.requestTemperatures(); // Send the command to get temperatures  
    LowPower.powerDown(SLEEP_500MS, ADC_OFF, BOD_OFF); // 9bit requres 95ms, 10bit 187ms, 11bit 375ms and 12bit resolution takes 750ms
    String message = "temperature1;";
    message += sensors.getTempC(tempAddress);
    digitalWrite(PIN_ALIM_TEMPERATURE, LOW); // turn DS18B20 sensor off
    pinMode(PIN_ALIM_TEMPERATURE, INPUT); // set power pin for DS18B20 to input before sleeping, saves power 

    // send
    message.getBytes(data, 32);
    Mirf.send(data);
    while (Mirf.isSending());
    
    // power down NRF to save energy
    Mirf.powerDown();
  }

  // sleep 8 secondes
  LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);
  cpt++;
}


