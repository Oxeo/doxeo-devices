// Enable debug prints to serial monitor
//#define MY_DEBUG

#define MY_RADIO_RF24
#define MY_PARENT_NODE_ID 4
#define MY_PARENT_NODE_IS_STATIC

// Enable IRQ pin
#define MY_RX_MESSAGE_BUFFER_FEATURE
#define MY_RF24_IRQ_PIN (2)

#include <MySensors.h>
#include <Servo.h>
#include <Parser.h>

#define SERVO_POWER_PIN 4
#define SERVO_PIN 5
#define BATTERY_LEVEL_PIN A0
#define EEPROM_VOLTAGE_CORRECTION 0
#define EEPROM_SERVO_POS 1
#define SERVO_UNLOCK_POS 41
#define SERVO_LOCK_POS 136

struct FuelGauge {
  float voltage;
  int percent;
};

enum state_enum {SLEEPING, RUNNING, GOING_TO_SLEEP};
uint8_t _state;

Servo _servo;
byte _servoPosition;
byte _servoTarget;
bool _servoMoving;
unsigned long _servoTimeChange = 0;
unsigned long _servoSpeed = 40;

Parser parser = Parser(' ');
MyMessage msg(0, V_CUSTOM);
unsigned long _goingToSleepTimer;
unsigned long _cpt = 0;

float _voltageCorrection = 1;
const int _lionTab[] = {3500, 3550, 3590, 3610, 3640, 3710, 3790, 3880, 3970, 4080, 4200};
int _batteryPercent = 101;

void before() {
  _servoPosition = loadState(EEPROM_SERVO_POS); // Set position to last known state (using eeprom storage)
  _servoMoving = false;

  //saveVoltageCorrection(1.013333333333333); // Measured by multimeter divided by reported
  _voltageCorrection = getVoltageCorrection();
  analogReference(INTERNAL);
  getFuelGauge(); // first read is wrong
  FuelGauge gauge = getFuelGauge();

  if (gauge.voltage <= 3.5) {
    sleep(0);
  }
}

void setup() {
  _state = SLEEPING;
  _cpt = 0;
  pinMode(SERVO_POWER_PIN, OUTPUT);
  digitalWrite(SERVO_POWER_PIN, LOW);

  _batteryPercent = 101;
  reportBatteryLevel();

  /*
  while(1) {
    delay(3000);
    reportBatteryLevel();
  }*/
}

void presentation() {
  sendSketchInfo("Bird Feeder", "2.0");
  present(0, S_CUSTOM);
}

void receive(const MyMessage &message)
{
  if (message.type == V_CUSTOM && message.sensor == 0) {
    parser.parse(message.getString());

    if (parser.isEqual(0, "close")) {
      if (!_servoMoving) {
        changeState(RUNNING);
        closeGate();
      }
    } else if (parser.isEqual(0, "open")) {
      if (!_servoMoving) {
        changeState(RUNNING);
        openGate();
      }
    } else if (parser.isEqual(0, "discover")) {
      transportRouteMessage(build(_msgTmp, 0, NODE_SENSOR_ID, C_INTERNAL,
                                            I_DISCOVER_RESPONSE).set(_transportConfig.parentNodeId));
    }
  }
}

void loop() {
  if (_state == SLEEPING) {
    sleep(1985);
    RF24_startListening();
    wait(15);
    _cpt += 1;

    if (_cpt == 21600) { // 6H
      _cpt = 0;
      reportBatteryLevel();
      sendHeartbeat();
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
  digitalWrite(SERVO_POWER_PIN, HIGH);
  delay(5);
}

void powerOffServo() {
  digitalWrite(SERVO_POWER_PIN, LOW);
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
        if (loadState(EEPROM_SERVO_POS) != _servoTarget) {
          saveState(EEPROM_SERVO_POS, _servoTarget);
        }

        delay(1000);
        _servo.detach();
        _servoMoving = false;
        powerOffServo();
        changeState(GOING_TO_SLEEP);
      }

      _servoTimeChange = millis();
    }
  }
}

void reportBatteryLevel() {
  FuelGauge gauge = getFuelGauge();

#ifdef MY_DEBUG
  Serial.print(F("Voltage: "));
  Serial.print(gauge.voltage);
  Serial.print(F(" ("));
  Serial.print(gauge.percent);
  Serial.println(F("%)"));
#endif

  if (gauge.percent < _batteryPercent) {
    String voltageMsg = "voltage-" + String(gauge.voltage) + "-" + String(gauge.percent);
    send(msg.set(voltageMsg.c_str()));
    sendBatteryLevel(gauge.percent);
    _batteryPercent = gauge.percent;
  }

  if (gauge.voltage <= 3.5) {
    powerOffServo();
    send(msg.set(F("Battery too low: sleep")));
    sleep(0);
  }
}

FuelGauge getFuelGauge() {
  FuelGauge gauge;
  int analog = analogRead(BATTERY_LEVEL_PIN);  // 0 - 1023
  float u2 = (analog * 1.1) / 1023.0;
  gauge.voltage = (u2 * (470000.0 + 100000.0)) / 100000.0;
  gauge.voltage *= _voltageCorrection;

  int um = round(gauge.voltage * 1000.0);

  for (byte i = 10; i >= 0; i--) {
    if (um >= _lionTab[i]) {
      if (i == 10) {
        gauge.percent = 100;
      } else {
        gauge.percent = map(um, _lionTab[i], _lionTab[i + 1], i * 10, i * 10 + 10);
      }

      break;
    } else {
      gauge.percent = 0;
    }
  }

  return gauge;
}

void saveVoltageCorrection(float value) {
  byte *b = (byte *)&value;

  for (byte i = 0; i < sizeof(value); i++) {
    saveState(EEPROM_VOLTAGE_CORRECTION + i, b[i]);
  }
}

float getVoltageCorrection() {
  float value = 0.0;
  byte *b = (byte *)&value;

  for (byte i = 0; i < sizeof(value); i++) {
    *b++ = loadState(EEPROM_VOLTAGE_CORRECTION + i);
  }

  return value;
}
