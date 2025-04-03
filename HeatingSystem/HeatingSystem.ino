// Enable debug prints to serial monitor
#define MY_DEBUG

// Enable and select radio type attached
#define MY_RADIO_RF24

// Enable IRQ pin
#define MY_RX_MESSAGE_BUFFER_FEATURE
#define MY_RF24_IRQ_PIN (2)

// RF24 PA level
#define MY_RF24_PA_LEVEL (RF24_PA_HIGH)

#include <MySensors.h>
#include <RGBLed.h>

#define RELAY 3
#define RED_LED A0
#define GREEN_LED A1
#define BLUE_LED A2

#define CHILD_ID_START_STOP_LEGACY 0
#define CHILD_ID_START_STOP 1

enum Status {
  OFF,
  COOL,
  HEAT
};

MyMessage _msgLegacy(CHILD_ID_START_STOP_LEGACY, V_CUSTOM);
MyMessage _msgStartStop(CHILD_ID_START_STOP, V_STATUS);
RGBLed _led(RED_LED, GREEN_LED, BLUE_LED, COMMON_CATHODE);
Status _status = OFF;
unsigned long _lastChangeStatus = 0;

void before() {
  // init PIN
  pinMode(RELAY, OUTPUT);

  // init status from eprom
  if (loadState(0) < 3) {
    setStatus(loadState(0));
  } else {
    setStatus(OFF);
  }
}

void presentation() {
  sendSketchInfo("HeatingSystem", "1.0");

  present(CHILD_ID_START_STOP_LEGACY, S_CUSTOM, "Legacy");
  present(CHILD_ID_START_STOP, S_BINARY, "Start/Stop heater");
}

void setup() {
  send(_msgLegacy.set(F("started")));
  send(_msgStartStop.set(_status == HEAT));
}

void receive(const MyMessage &message) {
  if (message.sensor == CHILD_ID_START_STOP_LEGACY && message.type == V_CUSTOM) {
    String str = message.getString();
    Status oldStatus = _status;

    if (str == "heat") {
      setStatus(HEAT);
      if (oldStatus != HEAT) {
        send(_msgLegacy.set(F("set to heat")));
        send(_msgStartStop.set(true));
      }
    } else if (str == "cool") {
      setStatus(COOL);
      if (oldStatus != COOL) {
        send(_msgLegacy.set(F("set to cool")));
        send(_msgStartStop.set(false));
      }
    } else if (str == "off") {
      setStatus(OFF);
      if (oldStatus != OFF) {
        send(_msgLegacy.set(F("set to off")));
        send(_msgStartStop.set(false));
      }
    }
  } else if (message.sensor == CHILD_ID_START_STOP && message.type == V_STATUS) {
    if (message.getBool()) {
      setStatus(HEAT);
    } else {
      setStatus(OFF);
    }
  }
}

void loop() {
  if (_status == HEAT && millis() - _lastChangeStatus > 7200000UL) {  // 2H
    setStatus(OFF);
    send(_msgLegacy.set(F("set to off after 2h")));
    send(_msgStartStop.set(false));
  }

  manageHeartbeat();
}

void setStatus(Status status) {
  if (_status != status) {
    saveState(0, status);
    _status = status;
  }

  switch (status) {
    case OFF:
      Serial.println("OFF status");
      _led.off();
      digitalWrite(RELAY, false);
      break;
    case COOL:
      Serial.println("COOL status");
      _led.setColor(RGBLed::BLUE);
      digitalWrite(RELAY, false);
      break;
    case HEAT:
      Serial.println("HEAT status");
      _led.setColor(RGBLed::RED);
      digitalWrite(RELAY, true);
      break;
  }

  _lastChangeStatus = millis();
}

inline void manageHeartbeat() {
  static unsigned long lastSend = 0;

  if (millis() - lastSend >= 3600000) {
    sendHeartbeat();
    lastSend = millis();
  }
}
