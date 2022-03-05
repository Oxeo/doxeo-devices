#ifndef BatteryLevel_h
#define BatteryLevel_h

#include "Arduino.h"
#include <Vcc.h>

#define INTERNAL_MEASUREMENT 255

enum BatteryType { Lithium, Alkaline, CR2032_LITHIUM };

class BatteryLevel
{
  public:
    BatteryLevel(int pinBatteryLevel, uint32_t eepromAddress, BatteryType batteryType);
    void init();
    void compute();
    float getVoltage();
    int getPercent();
    void saveVoltageCorrection(float value);
    float getVoltageCorrection();
    bool hasChanged();
    bool hasChanged(int gap);

  private:
    int _pinBatteryLevel;
    float _voltageCorrection;
    uint32_t _eepromAddress;
    float _voltage;
    int _percent;
    int _oldPercent;
    BatteryType _batteryType;
    Vcc *vcc;
};

#endif
