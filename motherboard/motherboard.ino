#include <RCSwitch.h>
#include <OxeoDio.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Timer.h>
#include <Mirf.h>

#include <SPI.h>      // Pour la communication via le port SPI
#include <Mirf.h>     // Pour la gestion de la communication
#include <nRF24L01.h> // Pour les définitions des registres du nRF24L01
#include <MirfHardwareSpiDriver.h> // Pour la communication SPI (ne cherchez pas à comprendre)
#include <DoxeoConfig.h>

#define PIN_LED_YELLOW 5
#define PIN_LED_RED 8
#define PIN_BUZZER A5
#define PIN_TEMPERATURE 4
#define PIN_RF_RECEIVER 2
#define PIN_RF_TRANSMITTER 3

RCSwitch mySwitch = RCSwitch();
OxeoDio dio = OxeoDio();
Timer timer;

// Temperature sensor
OneWire oneWire(PIN_TEMPERATURE);
DallasTemperature sensors(&oneWire);

// DIO buffer
unsigned long oldSenderDio = 0;
int timerIdDioReceptor = -1;

// RF buffer
unsigned long oldSenderRf = 0;
int timerIdRfReceptor = -1;

void setup() {
  pinMode(PIN_LED_YELLOW, OUTPUT);
  pinMode(PIN_LED_RED, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);

  mySwitch.enableReceive(digitalPinToInterrupt(PIN_RF_RECEIVER));
  mySwitch.enableTransmit(PIN_RF_TRANSMITTER);

  dio.setReceiverPin(PIN_RF_RECEIVER);
  dio.setSenderPin(PIN_RF_TRANSMITTER);

  // NRF init
  Mirf.cePin = 9; // Broche CE sur D9
  Mirf.csnPin = 10; // Broche CSN sur D10
  Mirf.spi = &MirfHardwareSpi; // On veut utiliser le port SPI hardware
  Mirf.init(); // Initialise la bibliothèque
  Mirf.channel = DOXEO_CHANNEL; // Choix du canal de communication (128 canaux disponibles, de 0 à 127)
  Mirf.payload = 32; // Taille d'un message (maximum 32 octets)
  Mirf.config(); // Sauvegarde la configuration dans le module radio
  Mirf.configRegister(RF_SETUP, 0x26); // to send much longeur
  Mirf.setRADDR((byte *) DOXEO_ADDR_MOTHER); // Adresse de réception

  Serial.begin(9600);

  sensors.begin();
  timer.every(600000, takeTemperature);

  timer.pulseImmediate(PIN_BUZZER, 100, HIGH);
}

byte nrfData[32];

void loop() {
  
  // Command reception
  if (Serial.available()) {
    String command = Serial.readStringUntil('\n');
    String commandType = getType(command);
    String commandName = getName(command);
    
    if (commandType == "nrf") {
      getValue(command).getBytes(nrfData, 32);
      byte addressDestination[6];
      commandName.getBytes(addressDestination, 6); // address on 5 charact
      Mirf.setTADDR(addressDestination); // Adresse de transmission
      Mirf.send(nrfData);
      while(Mirf.isSending());
      Serial.println("nrf command " + command + " send with success");
    } else if (commandType == "dio") {
      dio.send(getValue(command).toInt());
      Serial.println(command);
    } else if (commandType == "rf") {
      mySwitch.send(getValue(command).toInt(), 24);
      Serial.println(command);
    } else if (commandType == "box" && commandName == "temperature") {
      takeTemperature();
    } else if (commandType == "box" && commandName == "buzzer") {
      timer.pulseImmediate(PIN_BUZZER, getValue(command).toInt(), HIGH);
      Serial.println(command);
    } else if (commandType == "device_name") {
      send("device_name", "doxeo_board", "v1.0.0Beta1");
    } else {
      Serial.println("unknown command");
      timer.pulseImmediate(PIN_LED_RED, 500, HIGH);
    }
  }

  // NRF reception
  if (Mirf.dataReady()) {
    byte nrfMessage[32];
    timer.pulseImmediate(PIN_LED_YELLOW, 100, HIGH);
    Mirf.getData(nrfMessage);
    Serial.print("nrf;");
    Serial.println((char*) nrfMessage);
  }

  // DIO reception
  unsigned long sender = 0;
  if ((sender = dio.read()) != 0) {
    if (sender != oldSenderDio) {
      timer.pulseImmediate(PIN_LED_YELLOW, 100, HIGH);
      send("dio", "", sender);
      oldSenderDio = sender;
      if (timerIdDioReceptor != -1) {
        timer.stop(timerIdDioReceptor);
      }
      timerIdDioReceptor = timer.after(1000, resetTemponDio);
    }
  }

  // RF reception
  if (mySwitch.available()) {
    unsigned long sendValue = mySwitch.getReceivedValue();
    if (sendValue != 0 && sendValue != oldSenderRf) {
      timer.pulseImmediate(PIN_LED_YELLOW, 100, HIGH);
      send("rf", "", sendValue);
      oldSenderRf = sendValue;
      if (timerIdRfReceptor != -1) {
        timer.stop(timerIdRfReceptor);
      }
      timerIdRfReceptor = timer.after(1000, resetTemponRf);
    }
    mySwitch.resetAvailable();
  }

  // timer management
  timer.update();
}

void resetTemponDio() {
  oldSenderDio = 0;
  timerIdDioReceptor = -1;
}

void resetTemponRf() {
  oldSenderRf = 0;
  timerIdRfReceptor = -1;
}

void takeTemperature() {
  sensors.requestTemperatures();
  send("box", "temperature", sensors.getTempCByIndex(0));
}

void send(String type, String name, String value) {
  Serial.println(type + ";" + name + ";" + value);
}

void send(String type, String name, unsigned long value) {
  Serial.println(type + ";" + name + ";" + value);
}

void send(String type, String name, float value) {
  Serial.println(type + ";" + name + ";" + value);
}

String getType(String data) {
  return parseCommand(data,';',0);
}

String getName(String data) {
  return parseCommand(data,';',1);
}

String getValue(String data) {
  return parseCommand(data,';',2);
}

String parseCommand(String data, char separator, int index)
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

