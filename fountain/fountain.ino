#include <SPI.h>
#include <Mirf.h>
#include <nRF24L01.h>
#include <MirfHardwareSpiDriver.h>
#include <LowPower.h>
#include <DoxeoConfig.h>
#include <OneWire.h>           // Temperature sensor
#include <DallasTemperature.h> // Temperature sensor

//#define DEBUG
#include "DebugUtils.h"

#define WATER_SENSOR1 A1
#define WATER_PUMP1 7
#define WATER_SENSOR2 3 
#define WATER_PUMP2 6
#define LIGHT 8
#define PIN_ALIM_TEMPERATURE 4
#define PIN_TEMPERATURE 5
#define NRF_INTERRUPT 2
#define TEMPERATURE_PRECISION 11 //Max 12 bit, min 9 bit

// Timer
unsigned long pump1Timer = 0;
unsigned long pump2Timer = 0;
unsigned long lightTimer = 0;

// Token ID
unsigned long tokenId = 0;
unsigned long tokenIdTime = 0;

// Temperature sensor
OneWire oneWire(PIN_TEMPERATURE);
DallasTemperature sensors(&oneWire);
byte tempAddress[8];

// Status
bool lightOn;
bool pump1On;
bool pump2On;      
bool pump1IsRunning;
bool pump2IsRunning; 

// mode on off auto for pump1
unsigned long pump1AutoModeTimeOn;
unsigned long pump1AutoModeTimeOff;
unsigned long lastChangedWaterPump1Status;

void setup() {
  // init PIN
  pinMode(NRF_INTERRUPT, INPUT);
  pinMode(LIGHT, OUTPUT);
  pinMode(WATER_PUMP1, OUTPUT);
  pinMode(WATER_SENSOR1, INPUT_PULLUP);
  pinMode(WATER_PUMP2, OUTPUT);
  pinMode(WATER_SENSOR2, INPUT_PULLUP);
  
  digitalWrite(WATER_PUMP1, LOW);
  digitalWrite(WATER_PUMP2, HIGH);
  
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
  Mirf.configRegister(EN_RXADDR, 0x02); // only pipe 1 can received
  Mirf.configRegister(SETUP_RETR, 0x3F);  // retry 15x

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

  enablePump1(0);
  enablePump2(0);
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

    // parse message {receptorName};{id};{device}-{timeSelectedMinute}-{timeOnMinute}-{timeOffMinute}
    String from = parseMsg(msg,';', 0);
    String receptorName = parseMsg(msg,';', 1);
    int id = parseMsg(msg, ';', 2).toInt();
    String message = parseMsg(msg, ';', 3);
    String device = parseMsg(message, '-', 0);
    int timeSelectedMinute = parseMsg(message, '-', 1).toInt();
    int timeOnMinute = parseMsg(message, '-', 2).toInt();
    int timeOffMinute = parseMsg(message, '-', 3).toInt();

    // handle message
    if (receptorName == DOXEO_ADDR_MOTHER) {
        //redirect msg received to the motherboard
        sendNrf(msg);
    } else if (receptorName != DOXEO_ADDR_FOUNTAIN) {
      // do nothing, the message is not for us
    } else if (id == tokenId && (millis() - tokenIdTime < 60000)) {
      // already done, send success in case the previous message was not received
      sendAck(id);
    } else if (id == 0) {
      sendMessage("missing ID");
    } else if (message == "ping") {
      sendAck(id);
    } else if (device == "t1") {
      sendAck(id);
      String temp = takeTemperature();
      sendMessage("t1", temp);
    } else if (timeSelectedMinute < 0 || timeSelectedMinute > 720) {
      sendMessage("timeSelectedMinute arg error!");
    } else if (device == "l1") {
      sendAck(id);
      enableLight(timeSelectedMinute * 60);
    } else if (device == "p2") {
      sendAck(id);
      enablePump2(timeSelectedMinute * 60);
    } else if (device == "p1") {
      sendAck(id);
      enablePump1(timeSelectedMinute * 60, timeOnMinute *60, timeOffMinute * 60);
    } else {
      sendMessage("device arg error!");
    }
  } else if (lightOn || pump1On || pump2On) {

    if (lightOn && lightTimer < millis()) {
      enableLight(0);
    }

    if (pump1On && pump1Timer < millis()) {
      enablePump1(0);
    }
    
    if (pump2On && pump2Timer < millis()) {
      enablePump2(0);
    }
    
    if (pump1On) {
      // manage water sensor
      if (digitalRead(WATER_SENSOR1) == HIGH) {
        enablePump1(0);
        sendMessage("p1", "no water");
      }

      // manage on off auto mode
      if (pump1AutoModeTimeOn != 0 && pump1AutoModeTimeOff != 0) {
        if (pump1IsRunning && millis() > lastChangedWaterPump1Status + pump1AutoModeTimeOn) {
          startPump1(false);
          sendMessage("p1", "stand by");
        } else if (pump1IsRunning == false && millis() > lastChangedWaterPump1Status + pump1AutoModeTimeOff) {
          startPump1(true);
          sendMessage("p1", "stand by done");
        }
      }
    }
    
    if (pump2On) {
      // manage water sensor
      if (digitalRead(WATER_SENSOR2) == HIGH) {
        enablePump2(0);
        sendMessage("p2", "no water");
      }
    }
    
    sleep();
  
  } else {
    sleepForever();
  }
}

void startPump1(bool start) {
  if (start) {
    pump1IsRunning = true;
    digitalWrite(WATER_PUMP1, HIGH);
  } else {
    pump1IsRunning = false;
    digitalWrite(WATER_PUMP1, LOW);
  }
  lastChangedWaterPump1Status = millis();
}

void startPump2(bool start) {
  if (start) {
    pump2IsRunning = true;
    digitalWrite(WATER_PUMP2, HIGH);
  } else {
    pump2IsRunning = false;
    digitalWrite(WATER_PUMP2, LOW);
  }
}

void enablePump1(unsigned long durationSecond) {
  enablePump1(durationSecond, 0, 0);
}

void enablePump1(unsigned long durationSecond, unsigned long timeOnSecond, unsigned long timeOffSecond) {
  if (durationSecond > 0) {
    pinMode(WATER_SENSOR1, INPUT_PULLUP);
    startPump1(true);
    pump1Timer = millis() + durationSecond * 1000;
    pump1AutoModeTimeOn = timeOnSecond * 1000;
    pump1AutoModeTimeOff = timeOffSecond * 1000;
    pump1On = true;
    sendMessage("p1", "started");
  } else {
    startPump1(false);
    pinMode(WATER_SENSOR1, INPUT); // to save energy
    pump1Timer = 0;
    pump1On = false;
    sendMessage("p1", "stopped");
  }
}

void enablePump2(unsigned long durationSecond) {
  if (durationSecond > 0) {
    pinMode(WATER_SENSOR2, INPUT_PULLUP);
    startPump2(true);
    pump2Timer = millis() + durationSecond * 1000;
    pump2On = true;
    sendMessage("p2", "started");
  } else {
    startPump2(false);
    pinMode(WATER_SENSOR2, INPUT); // to save energy
    pump2Timer = 0;
    pump2On = false;
    sendMessage("p2", "stopped");
  }
}

void enableLight(unsigned long durationSecond) {
  if (durationSecond > 0) {
    digitalWrite(LIGHT, HIGH);
    lightTimer = millis() + durationSecond * 1000;
    lightOn = true;
    sendMessage("l1", "started");
  } else {
    digitalWrite(LIGHT, LOW);
    lightTimer = 0;
    lightOn = false;
    sendMessage("l1", "stopped");
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

void sendMessage(String device, String msg) {
  sendNrf(String(DOXEO_ADDR_FOUNTAIN) + '-' + device + ';' + String(DOXEO_ADDR_MOTHER) + ';' + msg);
}

void sendMessage(String msg) {
  sendNrf(String(DOXEO_ADDR_FOUNTAIN) + ';' + String(DOXEO_ADDR_MOTHER) + ';' + msg);
}

void sendNrf(String message) {
  DEBUG_PRINT("send message: " + message);
  byte data[32];
  message.getBytes(data, 32);
  Mirf.setTADDR((byte *) DOXEO_ADDR_MOTHER);
  Mirf.configRegister(EN_RXADDR, 0x03); // only pipe 0 and 1 can received
  for (int i=0; i<10; ++i) {
    Mirf.send(data);
    while (Mirf.isSending());
    if (Mirf.sendWithSuccess == true) {
      break;
    }
  }
  Mirf.configRegister(EN_RXADDR, 0x02); // only pipe 1 can received
}

void sleepForever() {
  if (digitalRead(NRF_INTERRUPT) == LOW) {
    Mirf.configRegister(STATUS, 0x70); // clear IRQ register
  }
  
  attachInterrupt(digitalPinToInterrupt(NRF_INTERRUPT), wakeUp, LOW);
  LowPower.powerDown(SLEEP_FOREVER, ADC_OFF, BOD_OFF);
  detachInterrupt(digitalPinToInterrupt(NRF_INTERRUPT));
  DEBUG_PRINT("WAKEUP");
}

void sleep() {
  if (digitalRead(NRF_INTERRUPT) == LOW) {
    Mirf.configRegister(STATUS, 0x70); // clear IRQ register
  }
  
  attachInterrupt(digitalPinToInterrupt(NRF_INTERRUPT), wakeUp, LOW);
  delay(1000);
  detachInterrupt(digitalPinToInterrupt(NRF_INTERRUPT));
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
