//#define MY_DEBUG
#define MY_RADIO_RF24
#define MY_RF24_PA_LEVEL (RF24_PA_MAX)

#define REPORT_BATTERY_LEVEL

#include <MySensors.h>

#define SOIL_ID 0
#define SOIL_PIN A0
#define SOIL_POWER A1

MyMessage msg(SOIL_ID, V_LEVEL);

#ifdef REPORT_BATTERY_LEVEL
#include <Vcc.h>
static uint8_t oldBatteryPcnt = 200;  // Initialize to 200 to assure first time value will be sent.
const float VccMin        = 2.8;      // Minimum expected Vcc level, in Volts: Brownout at 2.8V    -> 0%
const float VccMax        = 2.0 * 1.6; // Maximum expected Vcc level, in Volts: 2xAA fresh Alkaline -> 100%
const float VccCorrection = 1.0;      // Measured Vcc by multimeter divided by reported Vcc
static Vcc vcc(VccCorrection);
#endif

void setup()
{
  pinMode(SOIL_PIN, INPUT);
  pinMode(SOIL_POWER, OUTPUT);

  digitalWrite(SOIL_POWER, LOW);
}

void presentation()
{
  sendSketchInfo("Soil Moisture", "1.0");
  present(SOIL_ID, S_MOISTURE);
}

void loop()
{
  int soil = readSoil();

#ifdef MY_DEBUG
  Serial.print(F("Soil "));
  Serial.println(soil);
#endif
  
  delay(1); // fix mysensors bug
  for (char i = 0; i < 5; i++) {
    bool success = send(msg.set(soil));

    if (success) {
      i = 100;
    } else {
      delay(100);
    }
  }

#ifdef REPORT_BATTERY_LEVEL
  const uint8_t batteryPcnt = static_cast<uint8_t>(0.5 + vcc.Read_Perc(VccMin, VccMax));

#ifdef MY_DEBUG
  Serial.print(F("Vbat "));
  Serial.print(vcc.Read_Volts());
  Serial.print(F("\tPerc "));
  Serial.println(batteryPcnt);
#endif

  // Battery readout should only go down. So report only when new value is smaller than previous one.
  if ( batteryPcnt < oldBatteryPcnt )
  {
    sendBatteryLevel(batteryPcnt);
    oldBatteryPcnt = batteryPcnt;
  }
#endif

#ifdef MY_DEBUG
  delay(10000);
#else
  sleep(3600000); // 1H
#endif
}

int readSoil()
{
  digitalWrite(SOIL_POWER, HIGH);
  delay(100);
  int val = analogRead(SOIL_PIN);
  digitalWrite(SOIL_POWER, LOW);
  return val;
}
