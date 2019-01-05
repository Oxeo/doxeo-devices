//#define MY_DEBUG

#define REPORT_BATTERY_LEVEL

#define MY_RADIO_RF24
#define MY_RF24_PA_LEVEL (RF24_PA_MAX)
#define MY_TRANSPORT_MAX_TX_FAILURES (3u)

#include <MySensors.h>

#define DOORMAT_ID 0
#define DOORMAT_PIN 2
#define LED_PIN 4
//#define LED_ENABLE

MyMessage msg(DOORMAT_ID, V_STATUS);

#ifdef REPORT_BATTERY_LEVEL
#include <Vcc.h>
static uint8_t oldBatteryPcnt = 200;  // Initialize to 200 to assure first time value will be sent.
const float VccMin        = 2.8;      // Minimum expected Vcc level, in Volts: Brownout at 2.8V    -> 0%
const float VccMax        = 3.3; // Maximum expected Vcc level, in Volts: 2xAA fresh Alkaline -> 100%
const float VccCorrection = 1.0;      // Measured Vcc by multimeter divided by reported Vcc
static Vcc vcc(VccCorrection);
#endif

void setup() {
  // init PIN
  pinMode(DOORMAT_PIN, INPUT);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  for (char i = 0; i < 50; i++) {
    if (i % 2 == 0) {
      digitalWrite(LED_PIN, HIGH);
    } else {
      digitalWrite(LED_PIN, LOW);
    }

    delay(50);
  }
}

void presentation()
{
  sendSketchInfo("Doormat", "1.0");
  present(DOORMAT_ID, S_BINARY);
}

void loop() {
  uint8_t value;
  static uint8_t sentValue = 2;

  // Short delay to allow the sensor to properly settle
  delay(50);

  value = digitalRead(DOORMAT_PIN);

  if (value != sentValue) {

#ifdef LED_ENABLE
    if (value == HIGH) {
      digitalWrite(LED_PIN, HIGH);
    } else {
      digitalWrite(LED_PIN, LOW);
    }
#endif

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
  sleep(DOORMAT_PIN - 2, CHANGE, 0);
}

