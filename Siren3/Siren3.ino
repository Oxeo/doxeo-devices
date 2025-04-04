#define MY_DEBUG

#define MY_RADIO_RF24
#define MY_RX_MESSAGE_BUFFER_FEATURE
#define MY_RF24_IRQ_PIN (2)
#define MY_RX_MESSAGE_BUFFER_SIZE (15)
#define MY_REPEATER_FEATURE
#define MY_RF24_PA_LEVEL (RF24_PA_LOW)
#define MY_PARENT_NODE_ID 0
#define MY_PARENT_NODE_IS_STATIC

// Define pins
#define SIREN_PIN 5
#define POWER_PROBE_PIN A1
#define BATTERY_LEVEL_PIN A0

// Define EEPROM addresses
#define EEPROM_VOLTAGE_CORRECTION EEPROM_LOCAL_CONFIG_ADDRESS + 0  // 4 bytes storage
#define EEPROM_DELAY 4                                             // 4 bytes storage
#define EEPROM_SOUND_LEVEL 8                                       // 1 byte storage
#define EEPROM_BIP_NUMBER 9                                        // 1 byte storage

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
#define CHILD_ID_SIREN_LEGACY 0
#define CHILD_ID_MSG_RELAY_LEGACY 1
#define CHILD_ID_MSG_RELAY 2
#define CHILD_ID_SIREN_START_STOP 3
#define CHILD_ID_CONFIG_SIREN_DURATION 4
#define CHILD_ID_CONFIG_SOUND_LEVEL 5
#define CHILD_ID_CONFIG_BIP_NUMBER 6

// Siren
MyMessage msgSirenLegacy(CHILD_ID_SIREN_LEGACY, V_CUSTOM);
unsigned long _sirenTime = 0;
int _sirenState = 0;
byte _bipNumber = 0;
byte _sirenLevel = 100;
unsigned long _sirenDurationConfig = 0;
byte _soundLevelConfig = 100;
byte _bipNumberConfig = 0;

// Others
Parser parser = Parser(' ');
unsigned long _heartbeatTime = 0;
BatteryLevel battery(BATTERY_LEVEL_PIN, EEPROM_VOLTAGE_CORRECTION, Lithium);
bool _isOnBattery = false;

// Message relay
MyMessage msgRelayLegacy(CHILD_ID_MSG_RELAY_LEGACY, V_CUSTOM);
MyMessage msgRelay(CHILD_ID_MSG_RELAY, V_TEXT);
MyMessage msgToRelay;
unsigned long relayTimer = 0;

MyMessage msgSirenStartStop(CHILD_ID_SIREN_START_STOP, V_STATUS);
MyMessage msgSirenDuration(CHILD_ID_CONFIG_SIREN_DURATION, V_TEXT);
MyMessage msgSoundLevel(CHILD_ID_CONFIG_SOUND_LEVEL, V_TEXT);
MyMessage msgBipNumber(CHILD_ID_CONFIG_BIP_NUMBER, V_TEXT);

void before() {
  pinMode(SIREN_PIN, OUTPUT);
  pinMode(POWER_PROBE_PIN, INPUT);

  stopSiren();

  // battery.saveVoltageCorrection(0.9848130841121495); // Measured by multimeter divided by reported (with voltage correction = 1.0)
  battery.init();

  transportInit();  // needed to sleep RF24 module
  sleepIfBatteryToLow(false);

  _sirenDurationConfig = getSirenDuration();
  if (_sirenDurationConfig == 0) {
    _sirenDurationConfig = 180000UL;  // Default delay configuration in milliseconds (3 minutes)
  }

  _soundLevelConfig = getSoundLevel();
  if (_soundLevelConfig == 0 || _soundLevelConfig > 100) {
    _soundLevelConfig = 100;  // Default sound level in percentage
  }

  _bipNumberConfig = getBipNumber();
  if (_bipNumberConfig > 30) {
    _bipNumberConfig = 0;  // Default bip number
  }
}

void presentation() {
  sendSketchInfo("Siren 3", "1.1");

  present(CHILD_ID_SIREN_LEGACY, S_CUSTOM, "Siren legacy");
  present(CHILD_ID_MSG_RELAY_LEGACY, S_CUSTOM, "Msg relay legacy");
  present(CHILD_ID_MSG_RELAY, S_INFO, "Msg relay");
  present(CHILD_ID_SIREN_START_STOP, S_BINARY, "Start/Stop siren");
  present(CHILD_ID_CONFIG_SIREN_DURATION, S_INFO, "Siren duration (s)");
  present(CHILD_ID_CONFIG_SOUND_LEVEL, S_INFO, "Sound level (1 - 100)");
  present(CHILD_ID_CONFIG_BIP_NUMBER, S_INFO, "Bip number (0 - 30)");
}

void setup() {
  randomSeed(analogRead(3));  // A3

  send(msgSirenLegacy.set(F("system started")));
  send(msgSirenDuration.set("-"));
  send(msgSirenStartStop.set(false));
  send(msgSirenDuration.set((long)_sirenDurationConfig / 1000));
  send(msgSoundLevel.set(_soundLevelConfig));
  send(msgBipNumber.set(_bipNumberConfig));
}

void receive(const MyMessage &message) {
  if (message.sensor == CHILD_ID_SIREN_LEGACY && message.type == V_CUSTOM) {
    parser.parse(message.getString());

    if (parser.get(0) == NULL) {
      send(msgSirenLegacy.set(F("cmd missing! send help")));
    } else if (parser.isEqual(0, "ping")) {
      // nothing to do
    } else if (parser.isEqual(0, "stop")) {
      stopSiren();
      send(msgSirenLegacy.set(F("siren stopped by user")));
      send(msgSirenStartStop.set(false));
    } else if (parser.isEqual(0, "test")) {
      startSirenSound(100);
      delay(2000);
      stopSirenSound();
    } else if (parser.isEqual(0, "start") && parser.get(1) != NULL && parser.get(2) != NULL) {
      startSiren(parser.getInt(1), parser.getInt(2));
      send(msgSirenLegacy.set(F("siren started")));
    } else if (parser.isEqual(0, "help")) {
      send(msgSirenLegacy.set(F("cmd1: ping")));
      send(msgSirenLegacy.set(F("cmd2: stop")));
      send(msgSirenLegacy.set(F("cmd3: start [bip] [level]")));
    } else {
      send(msgSirenLegacy.set(F("command invalid")));
    }
  } else if (message.sensor == CHILD_ID_MSG_RELAY_LEGACY && message.type == V_CUSTOM) {
    relayMessage(&message);
  } else if (message.sensor == CHILD_ID_MSG_RELAY && message.type == V_TEXT) {
    relayMessage(&message);
  } else if (message.sensor == CHILD_ID_SIREN_START_STOP && message.type == V_STATUS) {
    if (message.getBool()) {
      startSiren(_bipNumberConfig, _soundLevelConfig);
    } else {
      stopSiren();
    }
  } else if (message.sensor == CHILD_ID_CONFIG_SIREN_DURATION && message.type == V_TEXT) {
    char *endPtr;
    unsigned long configTime = strtoul(message.getString(), &endPtr, 10);

    if (*endPtr == '\0') {
      _sirenDurationConfig = configTime * 1000UL;
      saveSirenDuration(_sirenDurationConfig);
      send(msgSirenDuration.set((long)_sirenDurationConfig / 1000));
    } else {
      send(msgSirenDuration.set("invalid"));
    }
  } else if (message.sensor == CHILD_ID_CONFIG_SOUND_LEVEL && message.type == V_TEXT) {
    char *endPtr;
    unsigned long soundLevel = strtoul(message.getString(), &endPtr, 10);

    if (*endPtr == '\0' && soundLevel > 0 && soundLevel <= 100) {
      _soundLevelConfig = (byte)soundLevel;
      saveSoundLevel(_soundLevelConfig);
      send(msgSoundLevel.set(_soundLevelConfig));
    } else {
      send(msgSoundLevel.set("invalid"));
    }
  } else if (message.sensor == CHILD_ID_CONFIG_BIP_NUMBER && message.type == V_TEXT) {
    char *endPtr;
    unsigned long bipNumber = strtoul(message.getString(), &endPtr, 10);

    if (*endPtr == '\0' && bipNumber <= 30) {
      _bipNumberConfig = (byte)bipNumber;
      saveBipNumber(_bipNumberConfig);
      send(msgBipNumber.set(_bipNumberConfig));
    } else {
      send(msgBipNumber.set("invalid"));
    }
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
      send(msgSirenLegacy.set(F("on power supply")));
      _isOnBattery = false;
    }
  } else {
    if (!_isOnBattery) {
      send(msgSirenLegacy.set(F("on battery")));
      lastSend = millis();
      _isOnBattery = true;
    } else {
      if (millis() - lastSend >= 3600000UL) {
        send(msgSirenLegacy.set(F("on battery")));
        lastSend = millis();
      }
    }
  }
}

void startSiren(byte bipBumber, byte level) {
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
    } else if ((_sirenState == _bipNumber * 2 + 2) && millis() - _sirenTime >= _sirenDurationConfig) {
      DEBUG_PRINT(F("Siren stopped by end of time!"));
      stopSiren();
      send(msgSirenLegacy.set(F("siren stopped")));
      send(msgSirenStartStop.set(false));
    }
  }
}

void startSirenSound(byte level) {
  analogWrite(SIREN_PIN, map(level, 0, 100, 0, 255));
}

void stopSirenSound() {
  analogWrite(SIREN_PIN, 0);
}

inline void manageHeartbeat() {
  if (millis() - _heartbeatTime >= 3600000) {
    sendHeartbeat();
    _heartbeatTime = millis();
  }
}

inline void manageBatteryLevel() {
  static unsigned long lastCheck = 0;
  static int lastBatteryPercent = 255;

  if (millis() - lastCheck > 10000UL) {
    lastCheck = millis();
    battery.compute();

    if ((_isOnBattery && battery.getPercent() < lastBatteryPercent) ||
        (!_isOnBattery && battery.getPercent() > lastBatteryPercent) ||
        lastBatteryPercent == 255) {
      //String msg = "battery: " + String(battery.getVoltage()) + "v " + String(battery.getPercent()) + "%";
      //send(msgSirenLegacy.set(msg.c_str()));
      sendBatteryLevel(battery.getPercent());
      lastBatteryPercent = battery.getPercent();
    }

    sleepIfBatteryToLow(true);
  }
}

void sleepIfBatteryToLow(bool sendMsg) {
  battery.compute();

  if (battery.getVoltage() <= 3.5) {
    if (sendMsg) {
      send(msgSirenLegacy.set("battery too low: sleep"));
      wait(500);
    }

    transportDisable();

    do {
      hwSleep(60000);
      battery.compute();
    } while (battery.getVoltage() <= 3.6);

    transportReInitialise();

    if (sendMsg) {
      send(msgSirenLegacy.set("battery ok: wakeup"));
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
    while (millis() - relayTimer < 2500 && !success) {
      success = transportSendWrite(msgToRelay.destination, msgToRelay);
      wait(5);
    }

    if (success) {
      send(msgRelayLegacy.set(F("success")));
      send(msgRelay.set(F("success")));
    } else {
      send(msgRelayLegacy.set(F("ko")));
      send(msgRelay.set(F("ko")));
    }
  }
}

int getMessagePart(const char *message, const byte index) {
  byte indexCount = 0;

  if (index == 0 && strlen(message) > 0) {
    return atoi(message);
  }

  for (byte i = 0; i < strlen(message) - 1; i++) {
    if (message[i] == '-') {
      indexCount++;
    }

    if (indexCount == index) {
      return atoi(message + i + 1);
    }
  }

  return 0;
}

const char *getPayload(const char *message) {
  byte indexCount = 0;

  for (byte i = 0; i < strlen(message) - 1; i++) {
    if (message[i] == '-') {
      indexCount++;
    }

    if (indexCount == 5) {
      return message + i + 1;
    }
  }

  return NULL;
}

void saveSirenDuration(unsigned long value) {
  byte *b = (byte *)&value;

  for (byte i = 0; i < sizeof(value); i++) {
    saveState(EEPROM_DELAY + i, b[i]);
  }
}

unsigned long getSirenDuration() {
  unsigned long value = 0;
  byte *b = (byte *)&value;

  for (byte i = 0; i < sizeof(value); i++) {
    *b++ = loadState(EEPROM_DELAY + i);
  }

  return value;
}

void saveSoundLevel(byte value) {
  saveState(EEPROM_SOUND_LEVEL, value);
}

byte getSoundLevel() {
  return loadState(EEPROM_SOUND_LEVEL);
}

void saveBipNumber(byte value) {
  saveState(EEPROM_BIP_NUMBER, value);
}

byte getBipNumber() {
  return loadState(EEPROM_BIP_NUMBER);
}