#ifndef BatteryLevel_h
#define BatteryLevel_h

#include "Arduino.h"

enum BatteryType { Lithium, Alkaline};

class BatteryLevel
{
  public:
    BatteryLevel(int pinBatteryLevel, uint32_t eepromAddress, BatteryType batteryType);
    void init();
    void compute();
    float getVoltage();
    int getPercent();
    void saveVoltageCorrection(float value);
    bool hasChanged();
    bool hasChanged(int gap);

  private:
    float getVoltageCorrection();
    
    int _pinBatteryLevel;
    float _voltageCorrection;
    uint32_t _eepromAddress;
    float _voltage;
    int _percent;
    int _oldPercent;
    BatteryType _batteryType;
};

#endif
