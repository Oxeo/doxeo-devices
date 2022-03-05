#include "batterylevel.h"
#include <EEPROM.h>

const int _lionTab[] = {3500, 3550, 3590, 3610, 3640, 3710, 3790, 3880, 3970, 4080, 4200};

BatteryLevel::BatteryLevel(int pinBatteryLevel, uint32_t eepromAddress, BatteryType batteryType)
{
  _pinBatteryLevel = pinBatteryLevel;
  _eepromAddress = eepromAddress;
  _batteryType = batteryType;
}

void BatteryLevel::init() {
  _voltageCorrection = getVoltageCorrection();
    
  if (_pinBatteryLevel == INTERNAL_MEASUREMENT) {
    vcc = new Vcc(_voltageCorrection);
  } else {
    pinMode(_pinBatteryLevel, INPUT);
    analogReference(INTERNAL);
  }
  
  compute(); // first read is wrong
}

void BatteryLevel::compute() {
  _oldPercent = _percent;
  
  if (_pinBatteryLevel == INTERNAL_MEASUREMENT) {
    _voltage = vcc->Read_Volts();
  } else {
    int analog = analogRead(_pinBatteryLevel);  // 0 - 1023
    float u2 = (analog * 1.1) / 1023.0;
    _voltage = (u2 * (470000.0 + 100000.0)) / 100000.0;
    _voltage *= _voltageCorrection;
  }
  
  int um = round(_voltage * 1000.0);

  if (_batteryType == Lithium) {
    for (int i = 10; i >= 0; i--) {
      if (um >= _lionTab[i]) {
        if (i == 10) {
          _percent = 100;
        } else {
          _percent = map(um, _lionTab[i], _lionTab[i + 1], i * 10, i * 10 + 10);
        }
    
        break;
      } else {
        _percent = 0;
      }
    }
  } else if (_batteryType == Alkaline) {
    _percent = map(um, 2800, 4800, 0, 100);
  } else if (_batteryType == CR2032_LITHIUM) {
    _percent = map(um, 2400, 3000, 0, 100);
  }

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

bool BatteryLevel::hasChanged(int gap) {
  return abs(_oldPercent - _percent) > gap;
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
