#include <SPI.h>
#include <Mirf.h>
#include <nRF24L01.h>
#include <MirfHardwareSpiDriver.h>
#include <LowPower.h>
#include <DoxeoConfig.h>
#include <OneWire.h>           // Temperature sensor
#include <DallasTemperature.h> // Temperature sensor

#define DEBUG
#include "DebugUtils.h"

#define WATER_SENSOR 3
#define WATER_PUMP 6
#define LIGHT 8
#define PIN_ALIM_TEMPERATURE 4
#define PIN_TEMPERATURE 5
#define NRF_INTERRUPT 2
#define TEMPERATURE_PRECISION 11 //Max 12 bit, min 9 bit

// Timer
unsigned long fountainTimer = 0;
unsigned long lightTimer = 0;

// Token ID
unsigned long tokenId = 0;
unsigned long tokenIdTime = 0;

// Temperature sensor
OneWire oneWire(PIN_TEMPERATURE);
DallasTemperature sensors(&oneWire);
byte tempAddress[8];

// Status
bool fountainOn;
bool lightOn;
bool waterPumpOn;

void setup() {
  // init PIN
  pinMode(NRF_INTERRUPT, INPUT);
  pinMode(LIGHT, OUTPUT);
  pinMode(WATER_PUMP, OUTPUT);
  pinMode(WATER_SENSOR, INPUT_PULLUP);
  
  // init serial for debugging
  #ifdef DEBUG
    Serial.begin(9600);
  #endif

  // init NRF
  Mirf.cePin = 9; // PIN CE sur D9
  Mirf.csnPin = 10; // PIN CSN sur D10
  Mirf.spi = &MirfHardwareSpi; // Hardware SPI: MISO -> 12, MOSI -> 11, SCK -> 13
  Mirf.init();
  Mirf.channel = DOXEO_CHANNEL; // Choix du canal de communication (128 canaux disponibles, de 0 à 127)
  Mirf.setRADDR((byte *) DOXEO_ADDR_FOUNTAIN);
  Mirf.payload = 32; // Taille d'un data (maximum 32 octets)
  Mirf.config(); // Sauvegarde la configuration dans le module radio
  Mirf.configRegister(RF_SETUP, 0x26); // sortie 0dBm @ 250Kbs to improve distance

  sendMessage("init started");

  // configure temperature sensor
  pinMode(PIN_ALIM_TEMPERATURE, OUTPUT);
  digitalWrite(PIN_ALIM_TEMPERATURE, HIGH);
  delay(50); // Allow 50ms for the sensor to be ready
  sensors.begin(); 
  sensors.setWaitForConversion(false);   
  int numSensors=sensors.getDeviceCount();
  oneWire.search(tempAddress);
  sensors.setResolution(tempAddress, TEMPERATURE_PRECISION);
  digitalWrite(PIN_ALIM_TEMPERATURE, LOW);
  pinMode(PIN_ALIM_TEMPERATURE, INPUT); // set power pin for DS18B20 to input before sleeping, saves power 

  enableFountain(10); // 10 seconds
  enableLight(5); // 5 seconds
  
  sendMessage("init done");
}


void loop() {  
  // message received
  if (Mirf.dataReady()) {
    // read message
    byte byteMessage[32];
    Mirf.getData(byteMessage);
    String msg = String((char*) byteMessage);
    DEBUG_PRINT("message received:" + msg);

    // parse message
    String receptorName = parseMsg(msg,';', 0);
    int id = parseMsg(msg, ';', 1).toInt();
    String message = parseMsg(msg, ';', 2);
    String device = parseMsg(message, '-', 0);
    int timeSelected = parseMsg(message, '-', 1).toInt();
    int mode = parseMsg(message, '-', 2).toInt();

    // handle message
    if (receptorName != DOXEO_ADDR_FOUNTAIN) {
      // do nothing, the message is not for us
    } else if (id == tokenId && (millis() - tokenIdTime < 60000)) {
      // already done, send success in case the previous message was not received
      sendAck(id);
    } else if (id == 0) {
      sendMessage("missing ID"); // NAME;ID
    } else if (message == "ping") {
      sendAck(id);
    } else if (device != "temperature" && device != "fountain" && device != "light") {
      sendMessage(String(id) + ";device arg error!");
    } else if (timeSelected < 1 || timeSelected > 360) {
      sendMessage(String(id) + ";timer selected arg error!");
    } else if (mode < 1 || mode > 1) {
      sendMessage(String(id) + ";mode arg error!");
    } else {
      sendAck(id);

      if (device == "temperature") {
        String temp = takeTemperature();
        sendMessage(temp);
      } else if (device == "fountain") {
        enableFountain(timeSelected * 60);
      } else if (device == "light") {
        enableLight(timeSelected * 60);
      }
    }
  } else if (lightOn || fountainOn) {

    if (lightOn && lightTimer < millis()) {
      enableLight(0);
    }

    if (fountainOn && fountainTimer < millis()) {
      enableFountain(0);
    }

    // manage water sensor
    if (fountainOn) {
      if (digitalRead(WATER_SENSOR) == HIGH && waterPumpOn) {
        enableWaterPump(false);
        sendMessage("Fountain stopped because not enough water detected!");
      } else if (digitalRead(WATER_SENSOR) == LOW && waterPumpOn == false) {
        enableWaterPump(true);
        sendMessage("Fountain started because enough water detected!");
      }
    }
    
    delay(100);
  
  } else {
    sleepForever();
  }
}

void enableWaterPump(bool enable) {
  if (enable) {
    waterPumpOn = true;
    digitalWrite(WATER_PUMP, LOW);
  } else {
    waterPumpOn = false;
    digitalWrite(WATER_PUMP, HIGH);
  }
}

void enableFountain(int durationSecond) {
  if (durationSecond > 0) {
    pinMode(WATER_SENSOR, INPUT_PULLUP);
    enableWaterPump(true);
    fountainTimer = millis() + durationSecond * 1000;
    fountainOn = true;
    sendMessage("Fountain started!");
  } else {
    enableWaterPump(false);
    pinMode(WATER_SENSOR, INPUT); // to save energy
    fountainTimer = 0;
    fountainOn = false;
    sendMessage("Fountain stopped!");
  }
}

void enableLight(int durationSecond) {
  if (durationSecond > 0) {
    digitalWrite(LIGHT, LOW); // start
    lightTimer = millis() + durationSecond * 1000;
    lightOn = true;
    sendMessage("Light started!");
  } else {
    digitalWrite(LIGHT, HIGH);  // stop
    lightTimer = 0;
    lightOn = false;
    sendMessage("Light stopped!");
  }
}

String takeTemperature() {
    pinMode(PIN_ALIM_TEMPERATURE, OUTPUT);
    digitalWrite(PIN_ALIM_TEMPERATURE, HIGH);
    LowPower.powerDown(SLEEP_30MS, ADC_OFF, BOD_OFF);
    sensors.setResolution(tempAddress, TEMPERATURE_PRECISION); 
    sensors.requestTemperatures(); // Send the command to get temperatures  
    LowPower.powerDown(SLEEP_500MS, ADC_OFF, BOD_OFF); // 9bit requres 95ms, 10bit 187ms, 11bit 375ms and 12bit resolution takes 750ms
    String temp = String(sensors.getTempC(tempAddress), 2);
    digitalWrite(PIN_ALIM_TEMPERATURE, LOW); // turn DS18B20 sensor off
    pinMode(PIN_ALIM_TEMPERATURE, INPUT); // set power pin for DS18B20 to input before sleeping, saves power

    return temp;
}

void sendAck(int id) {
  sendMessage(String(id) + ";success");
  tokenId = id;
  tokenIdTime = millis();
}

void sendMessage(String msg) {
  String message = String(DOXEO_ADDR_FOUNTAIN) + ';' + msg;
  DEBUG_PRINT("send message: " + message);
  byte data[32];
  message.getBytes(data, 32);
  Mirf.setTADDR((byte *) DOXEO_ADDR_MOTHER);
  for (int i=0; i<5; ++i) {
    Mirf.send(data);
    while (Mirf.isSending());
  }
}

void sleepForever() {
  attachInterrupt(digitalPinToInterrupt(NRF_INTERRUPT), wakeUp, LOW);
  LowPower.powerDown(SLEEP_FOREVER, ADC_OFF, BOD_OFF);
  detachInterrupt(digitalPinToInterrupt(NRF_INTERRUPT));
  Mirf.configRegister(STATUS, 0x70); // clear IRQ register
  DEBUG_PRINT("WAKEUP");
}

String parseMsg(String data, char separator, int index)
{
  int found = 0;
  int strIndex[] = {0, -1};
  int maxIndex = data.length()-1;

  for(int i=0; i<=maxIndex && found<=index; i++){
    if(data.charAt(i)==separator || i==maxIndex){
        found++;
        strIndex[0] = strIndex[1]+1;
        strIndex[1] = (i == maxIndex) ? i+1 : i;
    }
  }

  return found>index ? data.substring(strIndex[0], strIndex[1]) : "";
}

void wakeUp()
{
  // do nothing
}
