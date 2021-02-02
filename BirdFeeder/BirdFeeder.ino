// Enable debug prints to serial monitor
//#define MY_DEBUG

// Enable REPORT_BATTERY_LEVEL to measure battery level and send changes to gateway
#define REPORT_BATTERY_LEVEL

#define MY_RADIO_RF24
#define MY_PARENT_NODE_ID 4

// Enable IRQ pin
#define MY_RX_MESSAGE_BUFFER_FEATURE
#define MY_RF24_IRQ_PIN (2)

#include <MySensors.h>
#include <TimeLib.h>
#include <Servo.h>
#include <Parser.h>

#ifdef REPORT_BATTERY_LEVEL
#include <Vcc.h>
static uint8_t _oldBatteryPcnt = 200;  // Initialize to 200 to assure first time value will be sent.
const float _vccMin        = 2.8;      // Minimum expected Vcc level, in Volts: Brownout at 2.8V    -> 0%
const float _vccMax        = 2.0 * 1.6; // Maximum expected Vcc level, in Volts: 2xAA fresh Alkaline -> 100%
const float _vccCorrection = 1.0;      // Measured Vcc by multimeter divided by reported Vcc
static Vcc _vcc(_vccCorrection);
#endif

#define SERVO_POWER_PIN A0
#define SERVO_PIN 3
#define SERVO_UNLOCK_POS 52
#define SERVO_LOCK_POS 147

enum state_enum {SLEEPING, RUNNING, GOING_TO_SLEEP};
uint8_t _state;

Servo _servo;
byte _servoPosition;
byte _servoTarget;
bool _servoMoving;
unsigned long _servoTimeChange = 0;
unsigned long _servoSpeed = 10;

Parser parser = Parser(' ');
MyMessage msg(0, V_CUSTOM);
unsigned long _goingToSleepTimer;
unsigned long _cpt = 0;

void before() {
  _servoPosition = loadState(0); // Set position to last known state (using eeprom storage)
  _servoMoving = false;
}

void setup() {
  _state = SLEEPING;
  pinMode(SERVO_POWER_PIN, OUTPUT);
}

void presentation() {
  sendSketchInfo("Bird Feeder", "1.0");
  present(0, S_CUSTOM);
}

void receive(const MyMessage &message)
{
  if (message.type == V_CUSTOM && message.sensor == 0) {
    parser.parse(message.getString());

    if (parser.isEqual(0, "close")) {
      changeState(RUNNING);
      closeGate();
    } else if (parser.isEqual(0, "open")) {
      changeState(RUNNING);
      openGate();
    }
  }
}

void loop() {
  if (_state == SLEEPING) {
    sleep(1985);
    RF24_startListening();
    wait(15);
    _cpt += 1;

    if (_cpt % 21600UL == 0) { // 12H
      sendHeartbeat();
      reportBatteryLevel();
      changeState(GOING_TO_SLEEP);
    }
  } else if (_state == RUNNING) {
    manageServo();
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

void openGate() {
  powerOnServo();
  _servoTarget = SERVO_UNLOCK_POS;
  _servoMoving = true;
}

void closeGate() {
  powerOnServo();
  _servoTarget = SERVO_LOCK_POS;
  _servoMoving = true;
}

void powerOnServo() {
  //pinMode(SERVO_POWER_PIN, OUTPUT);
  digitalWrite(SERVO_POWER_PIN, HIGH);
  delay(5);
}

void powerOffServo() {
  digitalWrite(SERVO_POWER_PIN, LOW);
  //pinMode(SERVO_POWER_PIN, INPUT); // save power
}

inline void manageServo() {
  if (_servoMoving) {
    // start servo
    if (!_servo.attached()) {
      send(msg.set(F("moving gate...")));
      _servo.attach(SERVO_PIN);
      _servoTimeChange = millis();
    }

    // Move servo
    if (_servo.attached() && millis() - _servoTimeChange > _servoSpeed) {
      if (_servoTarget > _servoPosition) {
        _servoPosition++;
        _servo.write(_servoPosition);
      } else if (_servoTarget < _servoPosition) {
        _servoPosition--;
        _servo.write(_servoPosition);
      } else {
        if (_servoTarget == SERVO_UNLOCK_POS) {
          send(msg.set(F("Gate open")));
        } else {
          send(msg.set(F("Gate close")));
        }

        // save in eeprom
        if (loadState(0) != _servoTarget) {
          saveState(0, _servoTarget);
        }

        _servo.detach();
        _servoMoving = false;
        powerOffServo();
        changeState(GOING_TO_SLEEP);
      }

      _servoTimeChange = millis();
    }
  }
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
