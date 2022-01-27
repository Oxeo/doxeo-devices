#define MY_DEBUG

#define MY_RADIO_RF24
#define MY_RX_MESSAGE_BUFFER_FEATURE
#define MY_RF24_IRQ_PIN (2)
#define MY_RX_MESSAGE_BUFFER_SIZE (10)
#define MY_REPEATER_FEATURE
#define MY_RF24_PA_LEVEL (RF24_PA_MAX)
#define MY_PARENT_NODE_ID 0
#define MY_PARENT_NODE_IS_STATIC

// Define pins
#define SIREN_PIN 5
#define POWER_PROBE_PIN A1
#define BATTERY_LEVEL_PIN A0
#define EEPROM_VOLTAGE_CORRECTION 0

// Debug print
#if defined(MY_DEBUG)
#define DEBUG_PRINT(str) Serial.println(str);
#else
#define DEBUG_PRINT(str)
#endif

// Includes
#include <MySensors.h>
#include <Parser.h>

// Child ID
#define CHILD_ID_SIREN 0

// Siren
MyMessage msgSiren(CHILD_ID_SIREN, V_CUSTOM);
unsigned long _sirenTime = 0;
int _sirenState = 0;
byte _bipNumber = 0;
byte _sirenLevel = 100;

// Others
bool _isOnBattery = false;
Parser parser = Parser(' ');
unsigned long _heartbeatTime = 0;

// Message relay
MyMessage msgRelay(1, V_CUSTOM);
MyMessage msgToRelay;
unsigned long relayTimer = 0;

struct FuelGauge {
  float voltage;
  int percent;
};

float _voltageCorrection = 1;
const int _lionTab[] = {3500, 3550, 3590, 3610, 3640, 3710, 3790, 3880, 3970, 4080, 4200};
int _batteryPercent = 101;

unsigned long timer = 0;

void before()
{
  pinMode(SIREN_PIN, OUTPUT);
  pinMode(POWER_PROBE_PIN, INPUT);

  stopSiren();

  //saveVoltageCorrection(0.9848130841121495); // Measured by multimeter divided by reported
  _voltageCorrection = getVoltageCorrection();
  analogReference(INTERNAL);
  getFuelGauge(); // first read is wrong
  FuelGauge gauge = getFuelGauge();
}

void setup() {
  randomSeed(analogRead(3)); // A3

  _batteryPercent = 101;
  reportBatteryLevel();

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
  if (millis() - timer > 60000UL) {
    timer = millis();
      reportBatteryLevel();
  }
  
  manageSiren();
  managePowerProbe();
  manageHeartbeat();
}

inline void managePowerProbe() {
  if (digitalRead(POWER_PROBE_PIN) == HIGH) {
    if (_isOnBattery) {
      send(msgSiren.set(F("on power supply")));
      _isOnBattery = false;
    }
  } else {
    if (!_isOnBattery) {
      send(msgSiren.set(F("on battery")));
      _isOnBattery = true;
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

void reportBatteryLevel() {
  FuelGauge gauge = getFuelGauge();

#ifdef MY_DEBUG
  Serial.print(F("Voltage: "));
  Serial.print(gauge.voltage);
  Serial.print(F(" ("));
  Serial.print(gauge.percent);
  Serial.println(F("%)"));
#endif

  //if (gauge.percent < _batteryPercent) {
    String voltageMsg = "voltage-" + String(gauge.voltage) + "-" + String(gauge.percent);
    send(msgSiren.set(voltageMsg.c_str()));
    sendBatteryLevel(gauge.percent);
    _batteryPercent = gauge.percent;
  //}

  if (gauge.voltage <= 3.5) {
    send(msgSiren.set(F("Battery too low: sleep")));
    //sleep(0);
  }
}

FuelGauge getFuelGauge() {
  FuelGauge gauge;
  int analog = analogRead(BATTERY_LEVEL_PIN);  // 0 - 1023
  float u2 = (analog * 1.1) / 1023.0;
  gauge.voltage = (u2 * (470000.0 + 100000.0)) / 100000.0;
  gauge.voltage *= _voltageCorrection;

  int um = round(gauge.voltage * 1000.0);

  for (byte i = 10; i >= 0; i--) {
    if (um >= _lionTab[i]) {
      if (i == 10) {
        gauge.percent = 100;
      } else {
        gauge.percent = map(um, _lionTab[i], _lionTab[i + 1], i * 10, i * 10 + 10);
      }

      break;
    } else {
      gauge.percent = 0;
    }
  }

  return gauge;
}

void saveVoltageCorrection(float value) {
  byte *b = (byte *)&value;

  for (byte i = 0; i < sizeof(value); i++) {
    saveState(EEPROM_VOLTAGE_CORRECTION + i, b[i]);
  }
}

float getVoltageCorrection() {
  float value = 0.0;
  byte *b = (byte *)&value;

  for (byte i = 0; i < sizeof(value); i++) {
    *b++ = loadState(EEPROM_VOLTAGE_CORRECTION + i);
  }

  return value;
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

char* getPayload(const char* message) {
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
