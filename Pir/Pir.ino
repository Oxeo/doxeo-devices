// Enable debug prints to serial monitor
//#define MY_DEBUG
//#define LED_ENABLE

#define REPORT_BATTERY_LEVEL

#define MY_RADIO_RF24
#define MY_RF24_PA_LEVEL (RF24_PA_MAX)
#define MY_TRANSPORT_MAX_TX_FAILURES (3u)

#include <MySensors.h>

#define PIR_ID 0
#define PIR_PIN 3
#define LED_PIN 4

MyMessage msg(PIR_ID, V_TRIPPED);

#ifdef REPORT_BATTERY_LEVEL
#include <Vcc.h>
static uint8_t oldBatteryPcnt = 200;  // Initialize to 200 to assure first time value will be sent.
const float VccMin        = 2.8;      // Minimum expected Vcc level, in Volts: Brownout at 1.8V    -> 0%
const float VccMax        = 3.3; // Maximum expected Vcc level, in Volts: 2xAA fresh Alkaline -> 100%
const float VccCorrection = 1.0;      // Measured Vcc by multimeter divided by reported Vcc
static Vcc vcc(VccCorrection);
#endif

void before()
{
  pinMode(PIR_PIN, INPUT);
  pinMode(LED_PIN, OUTPUT);
  
  digitalWrite(LED_PIN, HIGH);
}

void setup()
{
  for (char i = 0; i < 50; i++) {
    if (i % 2 == 0) {
      digitalWrite(LED_PIN, HIGH);
    } else {
      digitalWrite(LED_PIN, LOW);
    }

    sleep(50);
  }

  pinMode(LED_PIN, INPUT);
}

void presentation()
{
  wait(500);
  sendSketchInfo("Motion Sensor", "1.0");
  wait(500);
  present(PIR_ID, S_MOTION);
}

void loop()
{
  bool tripped = digitalRead(PIR_PIN) == HIGH;

#ifdef LED_ENABLE
  pinMode(LED_PIN, OUTPUT);
  if (tripped) {
    digitalWrite(LED_PIN, HIGH);
  } else {
    digitalWrite(LED_PIN, LOW);
    pinMode(LED_PIN, INPUT);
  }
#endif

  for (char i = 1; i < 6; i++) {
    bool success = send(msg.set(tripped?"1":"0"));

    if (success) {
      break;
    } else {
      sleep(10 * i);
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
    delay(10);
    sendBatteryLevel(batteryPcnt);
    oldBatteryPcnt = batteryPcnt;
  }
#endif

  sleep(digitalPinToInterrupt(PIR_PIN), CHANGE, 0);
}
