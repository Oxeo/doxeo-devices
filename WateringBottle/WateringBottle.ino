#define MY_CORE_ONLY

#include "Arduino.h"
#include <EEPROM.h>
#include "BatteryLevel.h"
#include <SoftwareSerial.h>
#include <MySensors.h>

#define BLE_RX_PIN 5
#define BLE_TX_PIN 6
#define BLE_DATA_PIN 2
#define BLE_LINK_PIN 3
#define BLE_WKP_PIN 4
#define LED_PIN 8
#define PUMP_PIN 9
#define BATTERY_LEVEL_PIN A0
#define EEPROM_VOLTAGE_CORRECTION EEPROM_LOCAL_CONFIG_ADDRESS + 0 // 4 bytes storage
#define EEPROM_FREQUENCY EEPROM_LOCAL_CONFIG_ADDRESS + 4
#define EEPROM_AMOUNT_OF_WATER EEPROM_LOCAL_CONFIG_ADDRESS + 5

SoftwareSerial ble(BLE_TX_PIN, BLE_RX_PIN); // RX, TX
BatteryLevel battery(BATTERY_LEVEL_PIN, EEPROM_VOLTAGE_CORRECTION);

byte _linked = false;
int8_t _wakeupReason = 100;

bool _pumpIsOn = false;
unsigned long _pumpStartTime = 0;
unsigned long _pumpDuration = 0;

unsigned long _waitTimeMinute = 0;
unsigned long _remainingTimeMinute = 0;

void setup() {
  Serial.begin(9600);
  ble.begin(9600);

  pinMode(BLE_DATA_PIN, INPUT);
  pinMode(BLE_LINK_PIN, INPUT);
  pinMode(BLE_WKP_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  pinMode(PUMP_PIN, OUTPUT);

  //battery.saveVoltageCorrection(0.9849489795918367); // Measured by multimeter divided by reported (with voltage correction = 1.0)
  battery.init();
  battery.compute();

  Serial.println("Battery level: " + String(battery.getVoltage()) + "v (" + battery.getPercent() + "%)");
  Serial.println("Frequency: " + String(getFrequency()) + " day(s)");
  Serial.println("Amount of water: " + String(getAmountOfWater()) + " ml");

  startTimer();
  sleepBleDevice();
}

void loop() {
  if (_wakeupReason != MY_WAKE_UP_BY_TIMER) {
    while (ble.available()) {
      String msg = ble.readStringUntil('\n');
      Serial.println("ble: " + msg);
  
      if (msg == "cmd+start") {
        startPump(100);
      } else if (msg == "cmd+stop") {
        stopPump();
      } else if (msg == "cmd+data?") {
        Serial.println("Battery level: " + String(battery.getVoltage()) + "v (" + battery.getPercent() + "%)");
        ble.println("batvol:" + String(battery.getVoltage()));
        ble.println("batperc:" + String(battery.getPercent()));
        ble.println("frequency:" + String(getFrequency()));
        ble.println("water:" + String(getAmountOfWater()));
        ble.println("time:" + String(_remainingTimeMinute));
      } else if (msg.startsWith("cmd+frequency=")) {
        byte frequency = parseCommand(msg, '=', 1).toInt();
        saveFrequency(frequency);
        Serial.println("Frequency: " + String(frequency));
        ble.println("frequency:" + String(frequency));
        startTimer();
        ble.println("time:" + String(_remainingTimeMinute));
      } else if (msg.startsWith("cmd+restart")) {
        startTimer();
        ble.println("time:" + String(_remainingTimeMinute));
      } else if (msg.startsWith("cmd+water=")) {
        byte water = parseCommand(msg, '=', 1).toInt();
        saveAmountOfWater(water);
        Serial.println("Amount of water: " + String(water));
        ble.println("water:" + String(water));
      } else if (msg.startsWith("cmd+name=")) {
        String name = parseCommand(msg, '=', 1);
        Serial.println("Name: " + String(name));
        ble.print("+++");
        delay(500);
        ble.print("AT+NAME=[owb] " + name);
        delay(500);
        ble.print("AT+EXIT");
      } else if (msg == "cmd+try") {
        byte water = getAmountOfWater();
        startPump(water);
      }
    }
  
    if (Serial.available() > 0) {
      char incomingByte = Serial.read();
      ble.write(incomingByte);
    }

    manageBleDevice();
    managePump();
  }

  if (_linked == false && _pumpIsOn == false) {
    if (_wakeupReason != MY_WAKE_UP_BY_TIMER) {
      Serial.println("go to sleep");
      delay(100);
    }

    // Sleep until ble connected
    _wakeupReason  = sleep(digitalPinToInterrupt(BLE_LINK_PIN), RISING, 52000); // 60 secondes

    if (_wakeupReason == MY_WAKE_UP_BY_TIMER) {
      _remainingTimeMinute--;

      if (_remainingTimeMinute == 0) {
        Serial.println("It's time to water");
        _remainingTimeMinute = _waitTimeMinute;
        byte water = getAmountOfWater();
        startPump(water);
        _wakeupReason = 100;
      }
    }
    
    if (_wakeupReason != MY_WAKE_UP_BY_TIMER) {
      Serial.println("wake up");
    }
  }
}

void startTimer() {
  _waitTimeMinute = getFrequency() * 24 * 60;
  _remainingTimeMinute = _waitTimeMinute;
}

inline void managePump() {
  if (_pumpIsOn && millis() - _pumpStartTime >= _pumpDuration) {
    stopPump();
  }  
}

void startPump(unsigned long waterML) {
  digitalWrite(PUMP_PIN, HIGH);
  _pumpDuration = waterML * 0.95 * 1000UL;
  _pumpIsOn = true;
  _pumpStartTime = millis();
}

void stopPump() {
  digitalWrite(PUMP_PIN, LOW);
  _pumpIsOn = false;
}

void wakeupBleDevice() {
  digitalWrite(BLE_WKP_PIN, LOW);
  digitalWrite(LED_PIN, HIGH);
}

void sleepBleDevice() {
  digitalWrite(BLE_WKP_PIN, HIGH);
  digitalWrite(LED_PIN, LOW);
}

inline void manageBleDevice() {
  if (digitalRead(BLE_LINK_PIN) == HIGH && _linked == false) {
    _linked = true;
    wakeupBleDevice();
  } else if (digitalRead(BLE_LINK_PIN) == LOW && _linked == true) {
    _linked = false;
    sleepBleDevice();
  }
}

byte getFrequency() {
  byte result = EEPROM.read(EEPROM_FREQUENCY);

  if (result == 255) {
    result = 3;
  }

  return result;
}

byte getAmountOfWater() {
  byte result = EEPROM.read(EEPROM_AMOUNT_OF_WATER);

  if (result == 255) {
    result = 50;
  }

  return result;
}

void saveFrequency(byte value) {
  EEPROM.update(EEPROM_FREQUENCY, value);
}

void saveAmountOfWater(byte value) {
  EEPROM.update(EEPROM_AMOUNT_OF_WATER, value);
}

String parseCommand(String data, char separator, int index)
{
  int found = 0;
  int strIndex[] = {0, -1};
  int maxIndex = data.length() - 1;

  for (int i = 0; i <= maxIndex && found <= index; i++) {
    if (data.charAt(i) == separator || i == maxIndex || data.charAt(i) == '\n' || data.charAt(i) == '\r') {
      found++;
      strIndex[0] = strIndex[1] + 1;
      strIndex[1] = (i == maxIndex) ? i + 1 : i;
    }
  }

  return found > index ? data.substring(strIndex[0], strIndex[1]) : "";
}
