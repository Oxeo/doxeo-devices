#define MY_DEBUG

#define MY_RADIO_RF24
#define MY_RX_MESSAGE_BUFFER_FEATURE
#define MY_RF24_IRQ_PIN (2)
#define MY_RX_MESSAGE_BUFFER_SIZE (15)
#define MY_REPEATER_FEATURE
#define MY_RF24_PA_LEVEL (RF24_PA_MAX)
#define MY_PARENT_NODE_ID 0
#define MY_PARENT_NODE_IS_STATIC

// Define pins
#define SIREN_PIN 5
#define POWER_PROBE_PIN A1
#define BATTERY_LEVEL_PIN A0
#define EEPROM_VOLTAGE_CORRECTION EEPROM_LOCAL_CONFIG_ADDRESS + 0 // 4 bytes storage

// Debug print
#if defined(MY_DEBUG)
#define DEBUG_PRINT(str) Serial.println(str);
#else
#define DEBUG_PRINT(str)
#endif

// Includes
#include <MySensors.h>
#include <Parser.h>
#include "BatteryLevel.h"

// Child ID
#define CHILD_ID_SIREN 0

// Siren
MyMessage msgSiren(CHILD_ID_SIREN, V_CUSTOM);
unsigned long _sirenTime = 0;
int _sirenState = 0;
byte _bipNumber = 0;
byte _sirenLevel = 100;

// Others
Parser parser = Parser(' ');
unsigned long _heartbeatTime = 0;
BatteryLevel battery(BATTERY_LEVEL_PIN, EEPROM_VOLTAGE_CORRECTION, Lithium);
bool _isOnBattery = false;

// Message relay
MyMessage msgRelay(1, V_CUSTOM);
MyMessage msgToRelay;
unsigned long relayTimer = 0;

void before()
{
  pinMode(SIREN_PIN, OUTPUT);
  pinMode(POWER_PROBE_PIN, INPUT);

  stopSiren();

  //battery.saveVoltageCorrection(0.9848130841121495); // Measured by multimeter divided by reported (with voltage correction = 1.0)
  battery.init();

  transportInit(); // needed to sleep RF24 module
  sleepIfBatteryToLow(false);
}

void setup() {
  randomSeed(analogRead(3)); // A3
  send(msgSiren.set(F("system started")));
}

void presentation() {
  sendSketchInfo("Siren 3", "1.0");
  present(0, S_CUSTOM);
}

void receive(const MyMessage &myMsg)
{
  if (myMsg.type == V_CUSTOM && myMsg.sensor == 0) {
    parser.parse(myMsg.getString());

    if (parser.get(0) == NULL) {
      send(msgSiren.set(F("cmd missing! send help")));
    } else if (parser.isEqual(0, "ping")) {
      // nothing to do
    } else if (parser.isEqual(0, "stop")) {
      stopSiren();
      send(msgSiren.set(F("siren stopped by user")));
    } else if (parser.isEqual(0, "test")) {
      startSirenSound(100);
      delay(2000);
      stopSirenSound();
    } else if (parser.isEqual(0, "start") && parser.get(1) != NULL && parser.get(2) != NULL) {
      startSiren(parser.getInt(1), parser.getInt(2));
      send(msgSiren.set(F("siren started")));
    } else if (parser.isEqual(0, "help")) {
      send(msgSiren.set(F("cmd1: ping")));
      send(msgSiren.set(F("cmd2: stop")));
      send(msgSiren.set(F("cmd3: start [bip] [level]")));
    } else {
      send(msgSiren.set(F("command invalid")));
    }
  }

  if (myMsg.type == V_CUSTOM && myMsg.sensor == 1) {
    relayMessage(&myMsg);
  }
}

void loop() {
  manageSiren();
  managePowerProbe();
  manageBatteryLevel();
  manageHeartbeat();
}

inline void managePowerProbe() {
  static unsigned long lastSend = 0;

  if (digitalRead(POWER_PROBE_PIN) == HIGH) {
    if (_isOnBattery) {
      send(msgSiren.set(F("on power supply")));
      _isOnBattery = false;
    }
  } else {
    if (!_isOnBattery) {
      send(msgSiren.set(F("on battery")));
      lastSend = millis();
      _isOnBattery = true;
    } else {
      if (millis() - lastSend >= 3600000UL) {
        send(msgSiren.set(F("on battery")));
        lastSend = millis();
      }
    }
  }
}

void startSiren(char bipBumber, char level) {
  _bipNumber = bipBumber;
  _sirenLevel = level;
  _sirenTime = 0;
  _sirenState = 1;
  DEBUG_PRINT(F("Siren started!"));
}

void stopSiren() {
  _sirenState = 0;
  stopSirenSound();
  DEBUG_PRINT(F("Siren stopped!"));
}

bool isSirenOn() {
  return _sirenState != 0;
}

inline void manageSiren() {
  if (_sirenState != 0) {
    if ((_sirenState % 2 == 1 && _sirenState < _bipNumber * 2 + 2) && millis() - _sirenTime >= 1000) {
      startSirenSound(_sirenLevel);
      _sirenTime = millis();
      _sirenState++;
    } else if ((_sirenState % 2 == 0 && _sirenState < _bipNumber * 2 + 2) && millis() - _sirenTime >= 100) {
      stopSirenSound();
      _sirenTime = millis();
      _sirenState++;
    } else if ((_sirenState == _bipNumber * 2 + 2) && millis() - _sirenTime >= 180000) {
      DEBUG_PRINT(F("Siren stopped after 3 min!"));
      stopSiren();
      send(msgSiren.set(F("siren stopped")));
    }
  }
}

void startSirenSound(char level) {
  analogWrite(SIREN_PIN, map(level, 0, 100, 0, 255));
}

void stopSirenSound() {
  analogWrite(SIREN_PIN, 0);
}

inline void manageHeartbeat() {
  static unsigned long _heartbeatLastSend = 0;
  static unsigned long _heartbeatWait = random(1000, 60000);
  static unsigned long _heartbeatRetryNb = 0;

  if (millis() - _heartbeatLastSend >= _heartbeatWait) {
    bool success = sendHeartbeat();

    if (success) {
      _heartbeatWait = 45000;
      _heartbeatRetryNb = 0;
    } else {
      if (_heartbeatRetryNb < 10) {
        _heartbeatWait = random(100, 3000);
        _heartbeatRetryNb++;
      } else {
        _heartbeatWait = random(45000, 60000);
        _heartbeatRetryNb = 0;
      }
    }
    
    _heartbeatLastSend = millis();
  }
}

inline void manageBatteryLevel() {
  static unsigned long lastCheck = 0;
  
  if (millis() - lastCheck > 10000UL) {
    lastCheck = millis();
    battery.compute();
   
    if (battery.hasChanged(5)) {
      String msg = "battery:" + String(battery.getVoltage()) + "v (" + String(battery.getPercent()) + "%)";
      send(msgSiren.set(msg.c_str()));
    }

    sleepIfBatteryToLow(true);
  }
}

void sleepIfBatteryToLow(bool sendMsg) {
  battery.compute();
  
  if (battery.getVoltage() <= 3.5) {
      if (sendMsg) {
        send(msgSiren.set("battery too low: sleep"));
        wait(500);
      }
    
      transportDisable();
      
      do {
        hwSleep(60000);
        battery.compute();
      } while(battery.getVoltage() <= 3.6);

      transportReInitialise();

      if (sendMsg) {
        send(msgSiren.set("battery ok: wakeup"));
      }
  }
}

void relayMessage(const MyMessage *message) {
  if (getPayload(message->getString()) != NULL) {
      msgToRelay.destination = getMessagePart(message->getString(), 0);
      msgToRelay.sensor = getMessagePart(message->getString(), 1);
      mSetCommand(msgToRelay, getMessagePart(message->getString(), 2));
      mSetRequestAck(msgToRelay, getMessagePart(message->getString(), 3));
      msgToRelay.type = getMessagePart(message->getString(), 4);
      msgToRelay.sender = message->sender;
      mSetAck(msgToRelay, false);
      msgToRelay.set(getPayload(message->getString()));

      relayTimer = millis();
      bool success = false;
      while(millis() - relayTimer < 2500 && !success) {
        success = transportSendWrite(msgToRelay.destination, msgToRelay);
        wait(5);
      }

      if (success) {
        send(msgRelay.set(F("success")));
      } else {
        send(msgRelay.set(F("ko")));
      }
    }
}

int getMessagePart(const char* message, const byte index) {
  byte indexCount = 0;

  if (index == 0 && strlen(message) > 0) {
    return atoi(message);
  }
  
  for (byte i=0; i < strlen(message) - 1; i++) {
    if (message[i] == '-') {
      indexCount++;
    }

    if (indexCount == index) {
      return atoi(message + i + 1);
    }
  }

  return 0;
}

const char* getPayload(const char* message) {
  byte indexCount = 0;
  
  for (byte i=0; i < strlen(message) - 1; i++) {
    if (message[i] == '-') {
      indexCount++;
    }

    if (indexCount == 5) {
      return message + i + 1;
    }
  }

  return NULL;
}
