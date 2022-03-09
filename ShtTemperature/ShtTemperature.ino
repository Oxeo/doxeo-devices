// Enable debug prints to serial monitor
//#define MY_DEBUG

#define MY_RADIO_RF24
#define MY_RF24_PA_LEVEL (RF24_PA_MAX)

#define EEPROM_VOLTAGE_CORRECTION EEPROM_LOCAL_CONFIG_ADDRESS + 0 // 4 bytes storage

#include <MySensors.h>
#include <Wire.h>
#include "DFRobot_SHT20.h"
#include <BatteryLevel.h>

#define CHILD_ID_HUM  0
#define CHILD_ID_TEMP 1

MyMessage msgHum( CHILD_ID_HUM,  V_HUM);
MyMessage msgTemp(CHILD_ID_TEMP, V_TEMP);
DFRobot_SHT20 sht20;
BatteryLevel battery(INTERNAL_MEASUREMENT, EEPROM_VOLTAGE_CORRECTION, CR2032_LITHIUM);
int batteryPercent = 101;

void before()
{
  sht20.initSHT20();
  delay(100);

  //battery.saveVoltageCorrection(0.9958904109589041); // Measured by multimeter divided by reported (with voltage correction = 1.0)
  battery.init();
}

void setup()
{

}

void presentation()
{
  sendSketchInfo("SHT Temperature", "1.0");
  present(CHILD_ID_HUM, S_HUM, "Humidity");
  present(CHILD_ID_TEMP, S_TEMP, "Temperature");
}

void loop()
{
  const float temperature = sht20.readTemperature();
  const float humidity = sht20.readHumidity();
  send(msgTemp.set(temperature, 1));
  send(msgHum.set(humidity, 0));

  battery.compute();
  //String voltageMsg = "voltage-" + String(battery.getVoltage()) + "-" + String(battery.getPercent());
  //Serial.println(voltageMsg);
  if (battery.getPercent() < batteryPercent) {
    sendBatteryLevel(battery.getPercent());
    batteryPercent = battery.getPercent();
  }

  sleep(300000);
}
