// Enable debug prints to serial monitor
//#define MY_DEBUG

#define REPORT_BATTERY_LEVEL

#define MY_RADIO_RF24
#define MY_RF24_PA_LEVEL (RF24_PA_MAX)
#define MY_TRANSPORT_MAX_TX_FAILURES (3u)

#include <MySensors.h>

#define DOOR_ID 0
#define DOOR_PIN 2

MyMessage msg(DOOR_ID, V_TRIPPED);

#ifdef REPORT_BATTERY_LEVEL
#include <Vcc.h>
static uint8_t oldBatteryPcnt = 200;  // Initialize to 200 to assure first time value will be sent.
const float VccMin        = 2.8;      // Minimum expected Vcc level, in Volts: Brownout at 2.8V    -> 0%
const float VccMax        = 3.3; // Maximum expected Vcc level, in Volts: 2xAA fresh Alkaline -> 100%
const float VccCorrection = 1.0;      // Measured Vcc by multimeter divided by reported Vcc
static Vcc vcc(VccCorrection);
#endif

void setup()
{
  // Setup the buttons
  pinMode(DOOR_PIN, INPUT);

  // init PIN
  pinMode(A0, INPUT);
  pinMode(8, INPUT);
}

void presentation()
{
  wait(500);
  sendSketchInfo("Door 2", "1.4");
  wait(500);
  present(DOOR_ID, S_DOOR);
}

// Loop will iterate on changes on the BUTTON_PINs
void loop()
{
  uint8_t value;
  static uint8_t sentValue = 2;

  // Short delay to allow buttons to properly settle
  delay(50);

  value = digitalRead(DOOR_PIN);

  if (value != sentValue) {
    // Value has changed from last transmission, send the updated value
    for (char i = 0; i < 5; i++) {
      bool success = send(msg.set(value == HIGH));

      if (success) {
        i = 100;
      } else {
        delay(100);
      }
    }

    sentValue = value;
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
    delay(10);
    sendBatteryLevel(batteryPcnt);
    oldBatteryPcnt = batteryPcnt;
  }
#endif

  // Sleep until something happens with the sensor
  sleep(DOOR_PIN - 2, CHANGE, 0);
}
