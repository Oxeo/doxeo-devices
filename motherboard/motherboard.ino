#include <RCSwitch.h>
#include <OxeoDio.h>
#include <Timer.h>
#include "Nrf.h"

#include <DoxeoConfig.h>

#define PIN_LED_YELLOW A5
#define PIN_BUZZER 7
#define PIN_RF_RECEIVER 3
#define PIN_RF_TRANSMITTER 4
#define PIN_NRF_INTERRUPT 2
#define PIN_SWITCH0 A0
#define PIN_SWITCH1 A1
#define PIN_SWITCH2 A2

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

void setup() {
  // init pin
  pinMode(PIN_LED_YELLOW, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);
  pinMode(PIN_SWITCH0, OUTPUT);
  pinMode(PIN_SWITCH1, OUTPUT);
  pinMode(PIN_SWITCH2, OUTPUT);
  digitalWrite(PIN_LED_YELLOW, LOW);
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

  // init serial
  Serial.begin(9600);
  
  Serial.println("Doxeoboard started");

  // play buzzer
  timer.pulseImmediate(PIN_BUZZER, 20, HIGH);
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
    } else if (commandType == "switch") {
      if (commandValue == "on") {
        enableSwitch(commandName.toInt(), true);
      } else {
        enableSwitch(commandName.toInt(), false);
      }
      Serial.println(command);
    } else if (commandType == "name") {
      send("name", "doxeo_board", "v1.0.0");
    } else {
      Serial.println("error;unknown command: " + command);
    }
  }

  // DIO reception
  unsigned long sender = 0;
  if (!nrf.emergencySending() && (sender = dio.read()) != 0) {  // take 50ms
    if (sender != oldSenderDio) {
      timer.pulseImmediate(PIN_LED_YELLOW, 100, HIGH);
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
      timer.pulseImmediate(PIN_LED_YELLOW, 100, HIGH);
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

