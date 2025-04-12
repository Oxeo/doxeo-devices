#define MY_DEBUG
// #define DHT_SENSOR
#define PIR
// #define RF

#define MY_RADIO_RF24
#define MY_RX_MESSAGE_BUFFER_FEATURE
#define MY_RX_MESSAGE_BUFFER_SIZE (10)
#define MY_RF24_IRQ_PIN (2)
#define MY_RF24_PA_LEVEL (RF24_PA_HIGH)
#define MY_REPEATER_FEATURE

// Define pins
#define SIREN_PIN 7
#define GREEN_LED_PIN A1
#define RED_LED_PIN A0
#define POWER_PROBE_PIN A6
#define DHT_PIN 4
#define PIR_PIN A2
#define BUZZER_PIN A3
#define BUTTON1_PIN 5
#define BUTTON2_PIN 6
#define RF_PIN 3

// Debug print
#if defined(MY_DEBUG)
#define DEBUG_PRINT(str) Serial.println(str);
#else
#define DEBUG_PRINT(str)
#endif

// Includes
#include <Bounce2.h>
#include <MySensors.h>
#include <Parser.h>
#include <Timer.h>

// Child ID
#define CHILD_ID_SIREN 0
#define CHILD_ID_HUM 1
#define CHILD_ID_TEMP 2
#define CHILD_ID_PIR 3
#define CHILD_ID_RF 4
#define CHILD_ID_SIREN_START_STOP 5
#define CHILD_ID_CONFIG_SOUND_LEVEL 6
#define CHILD_ID_CONFIG_BIP_NUMBER 7
#define CHILD_ID_BUZZER 8
#define CHILD_ID_RED_LED 9

// Define EEPROM addresses
#define EEPROM_SOUND_LEVEL 0  // 1 byte storage
#define EEPROM_BIP_NUMBER 1   // 1 byte storage

// Siren
MyMessage msgSiren(CHILD_ID_SIREN, V_CUSTOM);
MyMessage msgSirenStartStop(CHILD_ID_SIREN_START_STOP, V_STATUS);
MyMessage msgSoundLevel(CHILD_ID_CONFIG_SOUND_LEVEL, V_TEXT);
MyMessage msgBipNumber(CHILD_ID_CONFIG_BIP_NUMBER, V_TEXT);
MyMessage msgBuzzer(CHILD_ID_BUZZER, V_STATUS);
MyMessage msgRedLed(CHILD_ID_RED_LED, V_STATUS);
unsigned long _sirenTime = 0;
int _sirenState = 0;
byte _bipNumber = 0;
byte _sirenLevel = 100;
byte _soundLevelConfig = 100;
byte _bipNumberConfig = 0;

// DHT
#if defined(DHT_SENSOR)
#include <DHT.h>
MyMessage msgHum(CHILD_ID_HUM, V_HUM);
MyMessage msgTemp(CHILD_ID_TEMP, V_TEMP);
DHT dht;
unsigned long lastSendTemperatureTime = 0;
#endif

// PIR
#if defined(PIR)
MyMessage msgPir(CHILD_ID_PIR, V_TRIPPED);
bool sendPirValue = false;
#endif

// RF 433MhZ
#if defined(RF)
#include <RCSwitch.h>
RCSwitch rcSwitch = RCSwitch();
unsigned long oldSenderRf = 0;
unsigned long oldSenderTfTime = 0;
MyMessage msgRf(CHILD_ID_RF, V_CUSTOM);
#endif

// Buttons
const byte buttons[] = {BUTTON1_PIN, BUTTON2_PIN};
Bounce* keys[sizeof(buttons)];
int button1OldValue = -1;
byte _password[4] = {1, 2, 2, 1};
int _passwordPosition = 0;
unsigned long _keyboardEnableTime = 0;

// Leds
bool greenLedOn = false;
bool redLedOn = false;

// Others
bool _isOnBattery = false;
Parser parser = Parser(' ');
Timer timer;
unsigned long _heartbeatTime = 0;
unsigned long _buzzerTimer = 0;

void before() {
  pinMode(SIREN_PIN, OUTPUT);
  pinMode(POWER_PROBE_PIN, INPUT);
  pinMode(PIR_PIN, INPUT);
  pinMode(GREEN_LED_PIN, OUTPUT);
  pinMode(RED_LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  stopSiren();
  setGreenLedOn(true);
  setRedLedOn(true);
  digitalWrite(BUZZER_PIN, LOW);

  _soundLevelConfig = getSoundLevel();
  if (_soundLevelConfig == 0 || _soundLevelConfig > 100) {
    _soundLevelConfig = 100;  // Default sound level in percentage
  }

  _bipNumberConfig = getBipNumber();
  if (_bipNumberConfig > 30) {
    _bipNumberConfig = 0;  // Default bip number
  }
}

void setup() {
  randomSeed(analogRead(3));  // A3
  setRedLedOn(false);
#if defined(DHT_SENSOR)
  dht.setup(DHT_PIN);
  sleep(dht.getMinimumSamplingPeriod());
#endif

#if defined(RF)
  rcSwitch.enableReceive(digitalPinToInterrupt(RF_PIN));
#endif

  for (byte i = 0; i < sizeof(buttons); i++) {
    keys[i] = new Bounce();
    keys[i]->attach(buttons[i], INPUT_PULLUP);
    keys[i]->interval(25);
  }

  for (byte i = 0; i < 10; i++) {
    if (i % 2 == 0) {
      setGreenLedOn(false);
      setRedLedOn(true);
    } else {
      setGreenLedOn(true);
      setRedLedOn(false);
    }
    delay(100);
  }

  send(msgSoundLevel.set(_soundLevelConfig));
  send(msgBipNumber.set(_bipNumberConfig));

  send(msgSiren.set(F("system started")));
  send(msgSirenStartStop.set(false));
  send(msgBuzzer.set(false));
  send(msgRedLed.set(false));

  setGreenLedOn(false);
  setRedLedOn(false);
}

void presentation() {
  sendSketchInfo("Siren 2", "1.1");
  present(CHILD_ID_SIREN, S_CUSTOM, "Info");
  present(CHILD_ID_HUM, S_HUM, "Humidity");
  present(CHILD_ID_TEMP, S_TEMP, "Temperature");
  present(CHILD_ID_PIR, S_MOTION, "Motion");
  present(CHILD_ID_RF, S_CUSTOM, "Rf");
  present(CHILD_ID_SIREN_START_STOP, S_BINARY, "Start/Stop siren");
  present(CHILD_ID_CONFIG_SOUND_LEVEL, S_INFO, "Sound level (1 - 100)");
  present(CHILD_ID_CONFIG_BIP_NUMBER, S_INFO, "Bip number (0 - 30)");
  present(CHILD_ID_BUZZER, S_BINARY, "Buzzer");
  present(CHILD_ID_RED_LED, S_BINARY, "Red led");
}

void receive(const MyMessage& myMsg) {
  if (myMsg.type == V_CUSTOM && myMsg.sensor == 0) {
    parser.parse(myMsg.getString());

    if (parser.get(0) == NULL) {
      send(msgSiren.set(F("cmd missing! send help")));
    } else if (parser.isEqual(0, "ping")) {
      // nothing to do
    } else if (parser.isEqual(0, "stop")) {
      stopSiren();
      send(msgSiren.set(F("siren stopped by user")));
      send(msgSirenStartStop.set(false));
    } else if (parser.isEqual(0, "start") && parser.get(1) != NULL && parser.get(2) != NULL) {
      startSiren(parser.getInt(1), parser.getInt(2));
      send(msgSiren.set(F("siren started")));
    } else {
      send(msgSiren.set(F("command invalid")));
    }
  } else if (myMsg.sensor == CHILD_ID_SIREN_START_STOP && myMsg.type == V_STATUS) {
    if (myMsg.getBool()) {
      startSiren(_bipNumberConfig, _soundLevelConfig);
    } else {
      stopSiren();
    }
  } else if (myMsg.sensor == CHILD_ID_CONFIG_SOUND_LEVEL && myMsg.type == V_TEXT) {
    char* endPtr;
    unsigned long soundLevel = strtoul(myMsg.getString(), &endPtr, 10);

    if (*endPtr == '\0' && soundLevel > 0 && soundLevel <= 100) {
      _soundLevelConfig = (byte)soundLevel;
      saveSoundLevel(_soundLevelConfig);
      send(msgSoundLevel.set(_soundLevelConfig));
    } else {
      send(msgSoundLevel.set("invalid"));
    }
  } else if (myMsg.sensor == CHILD_ID_CONFIG_BIP_NUMBER && myMsg.type == V_TEXT) {
    char* endPtr;
    unsigned long bipNumber = strtoul(myMsg.getString(), &endPtr, 10);

    if (*endPtr == '\0' && bipNumber <= 30) {
      _bipNumberConfig = (byte)bipNumber;
      saveBipNumber(_bipNumberConfig);
      send(msgBipNumber.set(_bipNumberConfig));
    } else {
      send(msgBipNumber.set("invalid"));
    }
  } else if (myMsg.sensor == CHILD_ID_BUZZER && myMsg.type == V_STATUS) {
    if (myMsg.getBool()) {
      digitalWrite(BUZZER_PIN, HIGH);
      _buzzerTimer = millis();
    } else {
      digitalWrite(BUZZER_PIN, LOW);
      _buzzerTimer = 0;
    }
  } else if (myMsg.sensor == CHILD_ID_RED_LED && myMsg.type == V_STATUS) {
    setRedLedOn(myMsg.getBool());
  }
}

void loop() {
  manageSiren();
  // managePowerProbe();
  manageDhtSensor();
  managePirSensor();
  manageRfReceptor();
  manageButtons();

  if (_buzzerTimer > 0 && millis() - _buzzerTimer >= 60000) {
    digitalWrite(BUZZER_PIN, LOW); // stop buzzer
    _buzzerTimer = 0;
    send(msgBuzzer.set(false));
  }

  manageHeartbeat();
  timer.update();
}

inline void manageRfReceptor() {
#if defined(RF)
  if (rcSwitch.available()) {
    unsigned long value = rcSwitch.getReceivedValue();
    if (value != 0 && (value != oldSenderRf || millis() - oldSenderTfTime > 1000)) {
      send(msgRf.set(value));
      oldSenderRf = value;
      oldSenderTfTime = millis();
    }
    rcSwitch.resetAvailable();
  }
#endif
}

inline void manageButtons() {
  // get pressed button number
  byte button = 0;
  for (byte i = 0; i < sizeof(buttons); i++) {
    keys[i]->update();
    if (keys[i]->fell()) {
      button += i + 1;
      _keyboardEnableTime = millis();
    }
  }

  // Button pressed
  if (button != -0) {
    DEBUG_PRINT(F("Button pressed:"));
    DEBUG_PRINT(button);

    if (button == _password[_passwordPosition]) {
      _passwordPosition++;
    } else {
      _passwordPosition = 0;
    }

    if (_passwordPosition == 4) {
      DEBUG_PRINT(F("Correct password entered!"));
      _passwordPosition = 0;
      timer.pulseImmediate(GREEN_LED_PIN, 3000, HIGH);

      if (isSirenOn()) {
        stopSiren();
        send(msgSiren.set(F("stopped by password")));
      } else {
        send(msgSiren.set(F("password entered")));
      }
    }
  }

  if (_passwordPosition != 0 && millis() - _keyboardEnableTime >= 2000) {
    _passwordPosition = 0;
  }
}

inline void manageDhtSensor() {
#if defined(DHT_SENSOR)
  if ((millis() - lastSendTemperatureTime) >= 600000) {
    lastSendTemperatureTime = millis();
    dht.readSensor(true);  // Force reading sensor, so it works also after sleep

    float temperature = dht.getTemperature();
    if (!isnan(temperature)) {
      send(msgTemp.set(temperature, 1));
    }

    float humidity = dht.getHumidity();
    if (!isnan(humidity)) {
      send(msgHum.set(humidity, 1));
    }
  }
#endif
}

inline void managePirSensor() {
#if defined(PIR)
  bool tripped = digitalRead(PIR_PIN) == HIGH;

  if (tripped != sendPirValue) {
    DEBUG_PRINT("PIR: event");
    send(msgPir.set(tripped ? "1" : "0"));
    sendPirValue = tripped;

    if (tripped) {
      setGreenLedOn(true);
    } else {
      setGreenLedOn(false);
    }
  }
#endif
}

inline void managePowerProbe() {
  // if plugged to sector
  if (analogRead(POWER_PROBE_PIN) > 400) {
    if (_isOnBattery) {
      DEBUG_PRINT(F("On power supply"));
      send(msgSiren.set(F("on power supply")));
      _isOnBattery = false;
    }
  } else {
    if (!_isOnBattery) {
      DEBUG_PRINT(F("On battery"));
      send(msgSiren.set(F("on battery")));
      _isOnBattery = true;
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
    } else if ((_sirenState == _bipNumber * 2 + 2) && millis() - _sirenTime >= 180000) {
      DEBUG_PRINT(F("Siren stopped after 180s!"));
      stopSiren();
      send(msgSiren.set(F("siren stopped")));
      send(msgSirenStartStop.set(false));
    }
  }
}

void startSirenSound(char level) {
  analogWrite(SIREN_PIN, map(level, 0, 100, 0, 255));
}

void stopSirenSound() {
  analogWrite(SIREN_PIN, 0);
}

void setGreenLedOn(bool on) {
  if (on) {
    digitalWrite(GREEN_LED_PIN, HIGH);
  } else {
    digitalWrite(GREEN_LED_PIN, LOW);
  }
}

void setRedLedOn(bool on) {
  if (on) {
    digitalWrite(RED_LED_PIN, HIGH);
  } else {
    digitalWrite(RED_LED_PIN, LOW);
  }
}

inline void manageHeartbeat() {
  static unsigned long heartbeatTime = 0;

  if (millis() - heartbeatTime >= 3600000) {
    sendHeartbeat();
    heartbeatTime = millis();
  }
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