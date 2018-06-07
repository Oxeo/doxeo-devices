#include <SPI.h>
#include <Mirf.h>
#include <nRF24L01.h>
#include <MirfHardwareSpiDriver.h>

#include <OneWire.h>           // Temperature sensor
#include <DallasTemperature.h> // Temperature sensor

#include <LowPower.h>
#include <DoxeoConfig.h>

//#define SENSOR_NAME "bedroom_temp"
#define SENSOR_NAME "lounge_temp"

#define PIN_TEMPERATURE 3 //4
#define PIN_ALIM_TEMPERATURE 4 //5
#define TEMPERATURE_PRECISION 11 //Max 12 bit, min 9 bit

// Temperature sensor
OneWire oneWire(PIN_TEMPERATURE);
DallasTemperature sensors(&oneWire);

int cpt = 0;
byte data[32];
byte tempAddress[8];

char* nodes[] = {DOXEO_ADDR_MOTHER, DOXEO_ADDR_SOUND, DOXEO_ADDR_FOUNTAIN};
const int nbNodes = 3;
int selectedNode = 0;

void setup() {
  randomSeed(analogRead(0));//initialise la séquence aléatoir

  // Configure NRF communication
  Mirf.cePin = 9; // Broche CE sur D9
  Mirf.csnPin = 10; // Broche CSN sur D10
  Mirf.spi = &MirfHardwareSpi;
  Mirf.init();
  Mirf.channel = 1; // Choix du canal de communication (128 canaux disponibles, de 0 à 127)
  Mirf.payload = 32; // Taille d'un data (maximum 32 octets)
  Mirf.config(); // Sauvegarde la configuration dans le module radio
  Mirf.configRegister(RF_SETUP, 0x26); // sortie 0dBm @ 250Kbs to improve distance
  Mirf.configRegister(SETUP_RETR, 0x3F);  // send retry 15 times
  Mirf.setTADDR((byte *) nodes[0]);

  // Send init message
  sendMessage("init started");

  // configure temperature sensor
  pinMode(PIN_ALIM_TEMPERATURE, OUTPUT);
  digitalWrite(PIN_ALIM_TEMPERATURE, HIGH);
  delay(50); // Allow 50ms for the sensor to be ready
  sensors.begin();
  sensors.setWaitForConversion(false);
  int numSensors = sensors.getDeviceCount();
  oneWire.search(tempAddress);
  sensors.setResolution(tempAddress, TEMPERATURE_PRECISION);
  digitalWrite(PIN_ALIM_TEMPERATURE, LOW);
  pinMode(PIN_ALIM_TEMPERATURE, INPUT); // set power pin for DS18B20 to input before sleeping, saves power

  cpt = 0;
  selectedNode = 0;

  sendMessage("init done");
}

void loop() {
  if (cpt % 112 == 0) {
    // take temperature
    pinMode(PIN_ALIM_TEMPERATURE, OUTPUT);
    digitalWrite(PIN_ALIM_TEMPERATURE, HIGH);
    LowPower.powerDown(SLEEP_30MS, ADC_OFF, BOD_OFF);
    sensors.setResolution(tempAddress, TEMPERATURE_PRECISION);
    sensors.requestTemperatures(); // Send the command to get temperatures
    LowPower.powerDown(SLEEP_500MS, ADC_OFF, BOD_OFF); // 9bit requres 95ms, 10bit 187ms, 11bit 375ms and 12bit resolution takes 750ms
    String temp = String(sensors.getTempC(tempAddress), 2);
    digitalWrite(PIN_ALIM_TEMPERATURE, LOW); // turn DS18B20 sensor off
    pinMode(PIN_ALIM_TEMPERATURE, INPUT); // set power pin for DS18B20 to input before sleeping, saves power

    // send
    sendMessage(temp);
  }

  // sleep 8 secondes
  LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);
  cpt++;
}

bool sendMessage(String msg) {
  return sendNrf(String(SENSOR_NAME) + ';' + msg);
}

bool sendNrf(String message) {
  message.getBytes(data, 32);

  for (unsigned char i=0; i<nbNodes; ++i) {
    // try to send 3 times
    for (unsigned char y=0; y<3; ++y) {
      Mirf.send(data);
      while (Mirf.isSending());
      Mirf.powerDown(); // power down NRF to save energy

      if (Mirf.sendWithSuccess == true) {
        break;
      } else {
        sleep(y + 1);
      }
    }

    if (Mirf.sendWithSuccess) {
      break;
    } else {
      selectedNode = (selectedNode + 1 < nbNodes) ? selectedNode + 1 : 0;
      Mirf.setTADDR((byte *) nodes[selectedNode]);
    }
  }
  
  return Mirf.sendWithSuccess;
}

void sleep(unsigned char nb) {
  for (unsigned char i = 0; i < nb; i++) {
    delay(random(1000)); // avoid colision
  }
}

