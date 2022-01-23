#ifndef BatteryLevel_h
#define BatteryLevel_h

#include "Arduino.h"

class BatteryLevel
{
  public:
    BatteryLevel(int pinBatteryLevel, uint32_t eepromAddress);
    void init();
    void compute();
    float getVoltage();
    int getPercent();
    void saveVoltageCorrection(float value);
    bool hasChanged();

  private:
    float getVoltageCorrection();
    
    int _pinBatteryLevel;
    float _voltageCorrection;
    uint32_t _eepromAddress;
    float _voltage;
    int _percent;
    int _oldPercent;
};

#endif
