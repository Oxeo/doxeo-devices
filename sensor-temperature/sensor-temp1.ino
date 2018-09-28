#include <SPI.h>
#include <Mirf.h>
#include <nRF24L01.h>
#include <MirfHardwareSpiDriver.h>

#include <OneWire.h>           // Temperature sensor
#include <DallasTemperature.h> // Temperature sensor

#include <LowPower.h>
#include <DoxeoConfig.h>

#define SENSOR_ID 1

#if SENSOR_ID == 1
  #define SENSOR_NAME "lounge_temp"
  #define PIN_TEMPERATURE 3
  #define PIN_ALIM_TEMPERATURE 4
#elif SENSOR_ID == 2
  #define SENSOR_NAME "bedroom_temp"
  #define PIN_TEMPERATURE 4
  #define PIN_ALIM_TEMPERATURE 5
#endif

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


void sendMessage(String msg) {
  bool success;

  for (int i = 0; i < 3; ++i) {
    success = sendNrf(String(SENSOR_NAME) + ';' + String(DOXEO_ADDR_MOTHER) + ";" + msg);

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

void sleep(unsigned char nb) {
  for (unsigned char i = 0; i < nb; i++) {
    delay(random(1000)); // avoid colision
  }
}

