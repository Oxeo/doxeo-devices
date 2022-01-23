#include "batterylevel.h"
#include <EEPROM.h>

BatteryLevel::BatteryLevel(int pinBatteryLevel, uint32_t eepromAddress)
{
  _pinBatteryLevel = pinBatteryLevel;
  _eepromAddress = eepromAddress;
}

void BatteryLevel::init() {
  pinMode(_pinBatteryLevel, INPUT);
  _voltageCorrection = getVoltageCorrection();
  analogReference(INTERNAL);
  compute(); // first read is wrong
}

void BatteryLevel::compute() {
  _oldPercent = _percent;
  int analog = analogRead(_pinBatteryLevel);  // 0 - 1023
  float u2 = (analog * 1.1) / 1023.0;
  _voltage = (u2 * (470000.0 + 100000.0)) / 100000.0;
  _voltage *= _voltageCorrection;

  int um = round(_voltage * 1000.0);
  _percent = map(um, 2800, 4800, 0, 100);

  if (_percent < 0) {
    _percent = 0;
  } else if (_percent > 100) {
    _percent = 100;
  }
}

float BatteryLevel::getVoltage() {
  return _voltage;
}

int BatteryLevel::getPercent() {
  return _percent;
}

bool BatteryLevel::hasChanged() {
  return _percent != _oldPercent;
}

void BatteryLevel::saveVoltageCorrection(float value) {
  byte *b = (byte *)&value;

  for (byte i = 0; i < sizeof(value); i++) {
    EEPROM.update(_eepromAddress + i, b[i]);
  }

  _voltageCorrection = value;
}

float BatteryLevel::getVoltageCorrection() {
  float value = 0.0;
  byte *b = (byte *)&value;

  for (byte i = 0; i < sizeof(value); i++) {
    *b++ = EEPROM.read(_eepromAddress + i);
  }

  if (isnan(value)) {
    value = 1.0;
  }

  return value;
}
