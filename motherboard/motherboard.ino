#include <RCSwitch.h>
#include <OxeoDio.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Timer.h>
#include <Mirf.h>
#include <QueueList.h> // https://playground.arduino.cc/Code/QueueList

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

// Timer management
Timer timer;

// Temperature sensor
OneWire oneWire(PIN_TEMPERATURE);
DallasTemperature sensors(&oneWire);

// DIO
OxeoDio dio = OxeoDio();
unsigned long oldSenderDio = 0;
int timerIdDioReceptor = -1;

// RF 433MhZ
RCSwitch rcSwitch = RCSwitch();
unsigned long oldSenderRf = 0;
int timerIdRfReceptor = -1;

// NRF
QueueList <String> nrfSendQueue;
byte nrfBufferToSend[32];
byte nrfByteAddress[6];
unsigned long nrfSendId = 1;
int nrfSendNumber = 0;
unsigned long nrfLastSendTime = 0;
String nrfSuccessMsgExpected = "";

//const int bt = 2;
unsigned long btTime = 0;
bool btPressed = false;

void setup() {
  // init pin
  pinMode(PIN_LED_YELLOW, OUTPUT);
  pinMode(PIN_LED_RED, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);
  //pinMode(bt, INPUT);

  // init RF 433MhZ
  rcSwitch.enableReceive(digitalPinToInterrupt(PIN_RF_RECEIVER));
  rcSwitch.enableTransmit(PIN_RF_TRANSMITTER);

  // init DIO
  dio.setReceiverPin(PIN_RF_RECEIVER);
  dio.setSenderPin(PIN_RF_TRANSMITTER);

  // init serial
  Serial.begin(9600);

  nrfSendQueue.setPrinter (Serial);

  // init NRF
  Mirf.cePin = 9; // Broche CE sur D9
  Mirf.csnPin = 10; // Broche CSN sur D10
  Mirf.spi = &MirfHardwareSpi; // Hardware SPI: MISO -> 12, MOSI -> 11, SCK -> 13
  Mirf.init(); // Initialise la bibliothèque
  Mirf.channel = DOXEO_CHANNEL; // Choix du canal de communication (128 canaux disponibles, de 0 à 127)
  Mirf.setRADDR((byte *) DOXEO_ADDR_MOTHER); // Adresse de réception
  Mirf.payload = 32; // Taille d'un message (maximum 32 octets)
  Mirf.config(); // Sauvegarde la configuration dans le module radio
  Mirf.configRegister(RF_SETUP, 0x26); // to send much longeur
  Mirf.configRegister(SETUP_RETR, 0x3F);  // retry 15x

  // init temperature sensor
  sensors.begin();
  delay(1000);
  timer.every(600000, takeTemperature);

  Serial.println("Doxeoboard started");

  // take first temperature
  takeTemperature();

  // play buzzer
  timer.pulseImmediate(PIN_BUZZER, 20, HIGH);
}

void loop() {
  
  // Command reception
  if (Serial.available()) {
    String command = Serial.readStringUntil('\n');
    String commandType = parseCommand(command, ';', 0);
    String commandName = parseCommand(command, ';', 1);
    String commandValue = parseCommand(command, ';', 2);
    
    if (commandType == "nrf") {
      nrfSendQueue.push(command);
    } else if (commandType == "dio") {
      dio.send(commandValue.toInt());
      Serial.println(command);
    } else if (commandType == "rf") {
      rcSwitch.send(commandValue.toInt(), 24);
      Serial.println(command);
    } else if (commandType == "box" && commandName == "temperature") {
      takeTemperature();
    } else if (commandType == "box" && commandName == "buzzer") {
      timer.pulseImmediate(PIN_BUZZER, commandValue.toInt(), HIGH);
      Serial.println(command);
    } else if (commandType == "name") {
      send("name", "doxeo_board", "v1.0.0");
    } else {
      Serial.println("error;unknown command: " + command);
      timer.pulseImmediate(PIN_LED_RED, 500, HIGH);
    }
  }
  
  /*if (!btPressed && digitalRead(bt) == HIGH && (millis() - btTime > 200)) {
    Serial.println("button pressed");
    nrfSendQueue.push("nrf;sound;1-1-10");
    
    btTime = millis();
    btPressed = true;
  }
  
  if (btPressed && digitalRead(bt) == LOW && (millis() - btTime > 200)) {
    btPressed = false;
  }*/

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
  if (rcSwitch.available()) {
    unsigned long sendValue = rcSwitch.getReceivedValue();
    if (sendValue != 0 && sendValue != oldSenderRf) {
      timer.pulseImmediate(PIN_LED_YELLOW, 100, HIGH);
      send("rf", "", sendValue);
      oldSenderRf = sendValue;
      if (timerIdRfReceptor != -1) {
        timer.stop(timerIdRfReceptor);
      }
      timerIdRfReceptor = timer.after(1000, resetTemponRf);
    }
    rcSwitch.resetAvailable();
  }
  
  // NRF reception
  while (Mirf.dataReady()) {
    byte byteMsg[32];
    Mirf.getData(byteMsg);
    String message = String((char *)byteMsg);
    timer.pulseImmediate(PIN_LED_YELLOW, 100, HIGH);
    
    // success returned no need to send again
    if (message == nrfSuccessMsgExpected) {
      nrfSendNumber = 0;
    } else {
      Serial.println("nrf;" + message);
    }
  };
  
  // Send Nrf message
  if (nrfSendNumber > 0 && (millis() - nrfLastSendTime > 500) && !Mirf.isSending()) {

    Mirf.setTADDR(nrfByteAddress);
    for (int i=0; i<10; ++i) {
      Mirf.send(nrfBufferToSend);
      while (Mirf.isSending());
      if (Mirf.sendWithSuccess == true) {
        int value = Mirf.getRetransmittedPackets();
        Serial.println("NRF message send: (" + String(value) + "x) " + String((char *)nrfBufferToSend));
        break;
      }
    }
    
    nrfSendNumber--;
    nrfLastSendTime = millis();
    
    // no success message received
    if (nrfSendNumber == 0) {
      Serial.println("error;the message " + String((char*) nrfBufferToSend) + " has not been received acknowledge!");
    }
  }
  
  // New Nrf message to send
  if (nrfSendNumber == 0 && !nrfSendQueue.isEmpty()) {
    String queue = nrfSendQueue.pop();
    
    // Set destination address
    parseCommand(queue,';',1).getBytes(nrfByteAddress, 6);
    
    // Prepare message to send
    String msgToSend = parseCommand(queue, ';', 1) + ";" + nrfSendId + ";" + parseCommand(queue, ';', 2);
    msgToSend.getBytes(nrfBufferToSend, 32);
    
    // prepare success message to be returned
    nrfSuccessMsgExpected = parseCommand(queue, ';', 1) + ";" + nrfSendId + ";success";
    
    // increase message ID
    nrfSendId++;
    if (nrfSendId == 0) {
      nrfSendId++;
    }
    
    nrfSendNumber = 10;
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
