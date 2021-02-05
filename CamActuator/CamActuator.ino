// Enable debug prints to serial monitor
//#define MY_DEBUG

// Enable REPORT_BATTERY_LEVEL to measure battery level and send changes to gateway
#define REPORT_BATTERY_LEVEL

#define MY_RADIO_RF24
//#define MY_PARENT_NODE_ID 4

// Enable IRQ pin
#define MY_RX_MESSAGE_BUFFER_FEATURE
#define MY_RF24_IRQ_PIN (2)

#include <MySensors.h>
#include <Parser.h>

#ifdef REPORT_BATTERY_LEVEL
#include <Vcc.h>
static uint8_t _oldBatteryPcnt = 200;  // Initialize to 200 to assure first time value will be sent.
const float _vccMin        = 2.8;      // Minimum expected Vcc level, in Volts: Brownout at 2.8V    -> 0%
const float _vccMax        = 2.0 * 1.6; // Maximum expected Vcc level, in Volts: 2xAA fresh Alkaline -> 100%
const float _vccCorrection = 1.0;      // Measured Vcc by multimeter divided by reported Vcc
static Vcc _vcc(_vccCorrection);
#endif

#define CAM_POWER_PIN A0

enum state_enum {SLEEPING, RUNNING, GOING_TO_SLEEP};
uint8_t _state;

Parser parser = Parser(' ');
MyMessage msg(0, V_CUSTOM);
unsigned long _goingToSleepTimer;
unsigned long _cpt = 0;
unsigned long waikTime = 10;
unsigned long startTime = 0;

void before() {

}

void setup() {
  _state = SLEEPING;
}

void presentation() {
  sendSketchInfo("Cam Actuator", "1.0");
  present(0, S_CUSTOM);
}

void receive(const MyMessage &message)
{
  if (message.type == V_CUSTOM && message.sensor == 0) {
    parser.parse(message.getString());

    if (parser.isEqual(0, "start")) {
      changeState(RUNNING);
      send(msg.set(F("started")));
      waikTime = parser.getInt(1);
      startCam();
    } else if (parser.isEqual(0, "stop")) {
      stopCam();
      send(msg.set(F("stopped")));
      changeState(GOING_TO_SLEEP);
    }
  }
}

void loop() {
  if (_state == SLEEPING) {
    sleep(1000);
    RF24_startListening();
    wait(15);
    _cpt += 1;

    if (_cpt % 21600UL == 0) { // 12H
      sendHeartbeat();
      reportBatteryLevel();
      changeState(GOING_TO_SLEEP);
    }
  } else if (_state == RUNNING) {
    if (millis() - startTime > waikTime * 1000UL) {
      stopCam();
      send(msg.set(F("stopped")));
      changeState(GOING_TO_SLEEP);
    }
  } else if (_state == GOING_TO_SLEEP) {
    if (millis() - _goingToSleepTimer > 500) {
      changeState(SLEEPING);
    }
  }
}

void changeState(uint8_t state) {
  switch (state) {
    case SLEEPING:
      break;
    case RUNNING:
      break;
    case GOING_TO_SLEEP:
      _goingToSleepTimer = millis();
      break;
  }

  _state = state;
}

void startCam() {
  pinMode(CAM_POWER_PIN, OUTPUT);
  digitalWrite(CAM_POWER_PIN, HIGH);
  startTime = millis();
}

void stopCam() {
  digitalWrite(CAM_POWER_PIN, LOW);
  pinMode(CAM_POWER_PIN, INPUT); // save power
}

inline void reportBatteryLevel() {
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
