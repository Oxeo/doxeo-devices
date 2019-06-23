#include <RCSwitch.h>
#include <OxeoDio.h>
#include <Timer.h>
#include <SoftwareSerial.h>
#include <DFRobotDFPlayerMini.h>
#include "Nrf.h"

#include <DoxeoConfig.h>

#define PIN_BUZZER 5
#define PIN_RF_RECEIVER 3
#define PIN_RF_TRANSMITTER 4
#define PIN_NRF_INTERRUPT 2
#define PIN_SWITCH0 A0
#define PIN_SWITCH1 A1
#define PIN_SWITCH2 A2
#define DFPLAYER_RX_PIN 6
#define DFPLAYER_TX_PIN 7

Nrf nrf(PIN_NRF_INTERRUPT);

// Timer management
Timer timer;

// DIO
OxeoDio dio = OxeoDio();
unsigned long oldSenderDio = 0;
int timerIdDioReceptor = -1;

// RF 433MhZ
RCSwitch rcSwitch = RCSwitch();
unsigned long oldSenderRf = 0;
int timerIdRfReceptor = -1;

// DF Player
SoftwareSerial dfPlayerSerial(DFPLAYER_RX_PIN, DFPLAYER_TX_PIN);
DFRobotDFPlayerMini dfPlayer;
unsigned long dfPlayerTimer = 0;

void setup() {
  // init pin
  pinMode(PIN_BUZZER, OUTPUT);
  pinMode(PIN_SWITCH0, OUTPUT);
  pinMode(PIN_SWITCH1, OUTPUT);
  pinMode(PIN_SWITCH2, OUTPUT);
  digitalWrite(PIN_BUZZER, LOW);
  digitalWrite(PIN_SWITCH0, LOW);
  digitalWrite(PIN_SWITCH1, LOW);
  digitalWrite(PIN_SWITCH2, LOW);

  nrf.init();

  // init RF 433MhZ
  rcSwitch.enableReceive(digitalPinToInterrupt(PIN_RF_RECEIVER));
  rcSwitch.enableTransmit(PIN_RF_TRANSMITTER);

  // init DIO
  dio.setReceiverPin(PIN_RF_RECEIVER);
  dio.setSenderPin(PIN_RF_TRANSMITTER);

  // init DFPlayer
  initDfPlayer();

  // init serial
  Serial.begin(9600);
  
  Serial.println("Doxeoboard started");
}

void loop() {
  
  // Command reception
  if (Serial.available()) {
    String command = Serial.readStringUntil('\n');
    String commandType = Nrf::parseCommand(command, ';', 0);
    String commandName = Nrf::parseCommand(command, ';', 1);
    String commandValue = Nrf::parseCommand(command, ';', 2);
    
    if (commandType == "nrf" || commandType == "nrf2") {
      nrf.sendMessage(command);
    } else if (commandType == "dio" && commandName != "") {
      dio.send(commandName.toInt());
      Serial.println(command);
    } else if (commandType == "rf" && commandName != "") {
      rcSwitch.send(commandName.toInt(), 24);
      Serial.println(command);
    } else if (commandType == "box" && commandName == "buzzer") {
      timer.pulseImmediate(PIN_BUZZER, commandValue.toInt(), HIGH);
      Serial.println(command);
    } else if (commandType == "box" && commandName == "sound") {
      int folder = Nrf::parseCommand(commandValue, '-', 0).toInt();
      int sound = Nrf::parseCommand(commandValue, '-', 1).toInt();
      int volume = Nrf::parseCommand(commandValue, '-', 2).toInt();

      if (folder != 0 && sound != 0 && volume != 0) {
        dfPlayer.volume(map(volume, 0, 100, 0, 30));
        dfPlayer.playFolder(folder, sound);
      } else {
        dfPlayer.stop();
      }
      
      Serial.println(command);
    } else if (commandType == "switch") {
      if (commandValue == "on") {
        enableSwitch(commandName.toInt(), true);
      } else {
        enableSwitch(commandName.toInt(), false);
      }
      Serial.println(command);
    } else if (commandType == "name") {
      send("name", "doxeo_board", "v2.0.0");
    } else {
      Serial.println("error;unknown command: " + command);
    }
  }

  // DIO reception
  unsigned long sender = 0;
  if (!nrf.emergencySending() && (sender = dio.read()) != 0) {  // take 50ms
    if (sender != oldSenderDio) {
      send("dio", String(sender), "event");
      oldSenderDio = sender;
      if (timerIdDioReceptor != -1) {
        timer.stop(timerIdDioReceptor);
      }
      timerIdDioReceptor = timer.after(1000, resetTemponDio);
    }
  }

  // RF reception
  if (!nrf.emergencySending() && rcSwitch.available()) {
    unsigned long sendValue = rcSwitch.getReceivedValue();
    if (sendValue != 0 && sendValue != oldSenderRf) {
      send("rf", String(sendValue), "event");
      oldSenderRf = sendValue;
      if (timerIdRfReceptor != -1) {
        timer.stop(timerIdRfReceptor);
      }
      timerIdRfReceptor = timer.after(1000, resetTemponRf);
    }
    rcSwitch.resetAvailable();
  }
  
  nrf.update();

   // print DFPlayer status
  if (millis() - dfPlayerTimer >= 1000 && dfPlayer.available()) {
    dfPlayerDetail(dfPlayer.readType(), dfPlayer.read());
    dfPlayerTimer = millis();
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

void enableSwitch(char id, boolean on) {
  if (id == 0) {
    if (on) {
        digitalWrite(PIN_SWITCH0, HIGH);
      } else {
        digitalWrite(PIN_SWITCH0, LOW);
      }
  } else if (id == 1) {
    if (on) {
        digitalWrite(PIN_SWITCH1, HIGH);
      } else {
        digitalWrite(PIN_SWITCH1, LOW);
      }
  } else if (id == 2) {
    if (on) {
        digitalWrite(PIN_SWITCH2, HIGH);
      } else {
        digitalWrite(PIN_SWITCH2, LOW);
      }
  }
}

void send(String type, String name, String value) {
  Serial.println(type + ";" + name + ";" + value);
}

void initDfPlayer() {
  dfPlayerSerial.begin(9600);

  if (!dfPlayer.begin(dfPlayerSerial)) {
    send(F("sound"), F("status"), F("Init error"));
  }

  dfPlayer.setTimeOut(500);
}

byte dfPlayerDetail(uint8_t type, int value) {
  switch (type) {
    case TimeOut:
      send(F("sound"), F("status"), F("time out"));
      return 1;
    case WrongStack:
      send(F("sound"), F("status"), F("wrong stack"));
      return 2;
    case DFPlayerCardInserted:
      send(F("sound"), F("status"), F("card inserted"));
      return 3;
    case DFPlayerCardRemoved:
      send(F("sound"), F("status"), F("card removed"));
      return 4;
    case DFPlayerCardOnline:
      send(F("sound"), F("status"), F("card online"));
      return 5;
    case DFPlayerPlayFinished:
      send(F("sound"), F("status"), F("play finished"));
      return 6;
    case DFPlayerError:
      switch (value) {
        case Busy:
          send(F("sound"), F("status"), F("card not found"));
          return 7;
        case Sleeping:
          send(F("sound"), F("status"), F("sleeping"));
          return 8;
        case SerialWrongStack:
          send(F("sound"), F("status"), F("get wrong ttack"));
          return 9;
        case CheckSumNotMatch:
          send(F("sound"), F("status"), F("checksum not match"));
          return 10;
        case FileIndexOut:
          send(F("sound"), F("status"), F("file index out of bound"));
          return 11;
        case FileMismatch:
          send(F("sound"), F("status"), F("cannot find file"));
          return 12;
        case Advertise:
          send(F("sound"), F("status"), F("in advertise"));
          return 13;
        default:
          send(F("sound"), F("status"), F("unknown error"));
          return 14;
      }
    default:
      return 0;
  }
}
