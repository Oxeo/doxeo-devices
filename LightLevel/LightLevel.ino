// Enable debug prints to serial monitor
//#define MY_DEBUG

// Enable REPORT_BATTERY_LEVEL to measure battery level and send changes to gateway
#define REPORT_BATTERY_LEVEL

#define MY_RADIO_RF24
#define MY_RF24_PA_LEVEL (RF24_PA_MAX)
#define MY_TRANSPORT_MAX_TX_FAILURES (3u)

#if defined(MY_DEBUG)
#define DEBUG_PRINT(str) Serial.println(str);
#else
#define DEBUG_PRINT(str)
#endif

#include <MySensors.h>

#define PIN_PHOTOCELL A3
#define PIN_ALIM_PHOTOCELL A5

#ifdef REPORT_BATTERY_LEVEL
#include <Vcc.h>
static uint8_t _oldBatteryPcnt = 200;  // Initialize to 200 to assure first time value will be sent.
const float _vccMin        = 2.8;      // Minimum expected Vcc level, in Volts: Brownout at 2.8V    -> 0%
const float _vccMax        = 2.0 * 1.6; // Maximum expected Vcc level, in Volts: 2xAA fresh Alkaline -> 100%
const float _vccCorrection = 1.0;      // Measured Vcc by multimeter divided by reported Vcc
static Vcc _vcc(_vccCorrection);
#endif

int lastPhotocell = -100;

MyMessage msgLight(0, V_LIGHT_LEVEL);

void setup() {
  pinMode(PIN_PHOTOCELL, INPUT);
}

void presentation() {
  sendSketchInfo("Light Level", "1.1");

  // Present sensor to controller
  present(0, S_LIGHT_LEVEL);
}

void loop() {
  int photocell = readPhotocell();
  DEBUG_PRINT(F("Light: "));
  DEBUG_PRINT(photocell);

  int diff = photocell - lastPhotocell;
  if (abs(diff) > 5) { 
    send(msgLight.set(photocell));
    lastPhotocell = photocell;
  }

  reportBatteryLevel();
  sendHeartbeat();
  sleep(60000);
}

int readPhotocell() {
  pinMode(PIN_ALIM_PHOTOCELL, OUTPUT);
  digitalWrite(PIN_ALIM_PHOTOCELL, HIGH);
  sleep(30);

  int value = analogRead(PIN_PHOTOCELL);
  value = map(value, 0, 1023, 0, 100);

  digitalWrite(PIN_ALIM_PHOTOCELL, LOW);
  pinMode(PIN_ALIM_PHOTOCELL, INPUT);

  return value;
}

void reportBatteryLevel() {
#ifdef REPORT_BATTERY_LEVEL
  const uint8_t batteryPcnt = static_cast<uint8_t>(0.5 + _vcc.Read_Perc(_vccMin, _vccMax));

#ifdef MY_DEBUG
  Serial.print(F("Vbat "));
  Serial.print(_vcc.Read_Volts());
  Serial.print(F("\tPerc "));
  Serial.println(batteryPcnt);
#endif

  // Battery readout should only go down. So report only when new value is smaller than previous one.
  if ( batteryPcnt < _oldBatteryPcnt )
  {
    sendBatteryLevel(batteryPcnt);
    _oldBatteryPcnt = batteryPcnt;
  }
#endif
}
