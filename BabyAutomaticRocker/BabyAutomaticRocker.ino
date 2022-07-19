#include "Arduino.h"
#include <EEPROM.h>
#include <TMC2208Stepper.h>
#include <SoftwareSerial.h>

#define MICROSTEP 200 * 2
#define STEP_DIST_RATIO 1000/150 // step numbers divided by robot distance (millimeter)
#define ACCELERATION_FREQUENCY 100.0 // milliseconds
#define ACCELERATION_SPEED (500.0 / (1000.0 / ACCELERATION_FREQUENCY)) // 60 RPM of acceleration in 1 second

#define MOTOR1_DIR_PIN 8
#define MOTOR1_STEP_PIN 4
#define MOTOR1_SLP_PIN 9
#define MOTOR1_UART_PIN A2

#define MOTOR2_DIR_PIN 11
#define MOTOR2_STEP_PIN 10
#define MOTOR2_SLP_PIN 12

#define BLE_RX_PIN 5
#define BLE_TX_PIN 6
#define BLE_LINK_PIN 7
#define BLE_DATA_PIN 3

#define EEPROM_DISTANCE 0       // 4 bytes
#define EEPROM_SPEED 4          // 4 bytes
#define EEPROM_TIMER 8          // 4 bytes
#define EEPROM_ACCELERATION 12  // 1 byte

enum State_enum {FIX, ACCELERATION, CRUISE, DECELERATION};
enum move2_enum {FORWARD, BACKWARD};

TMC2208Stepper driver = TMC2208Stepper(-1, MOTOR1_UART_PIN, false);
SoftwareSerial ble(BLE_TX_PIN, BLE_RX_PIN); // RX, TX

// to define
unsigned long speed = 160;       // RPM
float accelerationPercent = 10.0;
unsigned long lapDistance = 500;    // millimeter

unsigned long previousTime = micros();
unsigned long stateTimer = millis();
unsigned long stepperFrequency;
float rpmAcceleration;
float currentSpeed; // RPM
unsigned long stepCpt;
unsigned long nbStepForALap;
unsigned long nbStepToCount;
uint8_t state = FIX;
uint8_t move2;
int lapsNumber;

void setup()
{
  Serial.begin(9600);
  ble.begin(9600);

  initMotors();

  pinMode(BLE_LINK_PIN, INPUT);
  pinMode(BLE_DATA_PIN, INPUT);

  pinMode(MOTOR1_SLP_PIN, OUTPUT);
  pinMode(MOTOR2_SLP_PIN, OUTPUT);
  pinMode(MOTOR1_DIR_PIN, OUTPUT);
  pinMode(MOTOR2_DIR_PIN, OUTPUT);
  pinMode(MOTOR1_STEP_PIN, OUTPUT);
  pinMode(MOTOR2_STEP_PIN, OUTPUT);

  wakeUpMotors();

  //stealthChop2Autotune();
  //delay(200);

  lapsNumber = 0;
  initBeforeStartingANewLap();
  //measureStepDistanceRatio();
}

void loop()
{
  if (lapsNumber < 1) {
    moveRobot();
  }

  if (digitalRead(BLE_DATA_PIN) == HIGH) {
    while (ble.available()) {
      String msg = ble.readStringUntil('\n');
      Serial.println("ble: " + msg);

      if (msg == "cmd+start") {
        ble.println("status:on");
        lapsNumber = 0;
      } else if (msg == "cmd+stop") {
        ble.println("status:off");
        lapsNumber = 100;
      } else if (msg.startsWith("cmd+dist=")) {
        int distance = parseCommand(msg, '=', 1).toInt();
        saveDistance(distance);
        Serial.println("Distance: " + String(distance));
        ble.println("distance:" + String(distance));
      } else if (msg.startsWith("cmd+speed=")) {
        int speed = parseCommand(msg, '=', 1).toInt();
        saveSpeed(speed);
        Serial.println("Speed: " + String(speed));
        ble.println("speed:" + String(speed));
      } else if (msg.startsWith("cmd+accel=")) {
        int acceleration = parseCommand(msg, '=', 1).toInt();
        saveAcceleration(acceleration);
        Serial.println("Acceleration: " + String(acceleration));
        ble.println("acceleration:" + String(acceleration));
      } else if (msg.startsWith("cmd+timer=")) {
        byte timer = parseCommand(msg, '=', 1).toInt();
        saveTimer(timer);
        Serial.println("Timer: " + String(timer));
        ble.println("timer:" + String(timer));
      }
    }
  }

  /*if (Serial.available() > 0) {
    char incomingByte = Serial.read();
    ble.write(incomingByte);
    }*/
}

inline void moveRobot() {
  unsigned long currentTime = micros();

  if (currentTime - previousTime >= stepperFrequency) {
    previousTime = currentTime;

    digitalWrite(MOTOR1_STEP_PIN, HIGH);
    digitalWrite(MOTOR2_STEP_PIN, HIGH);
    digitalWrite(MOTOR1_STEP_PIN, LOW);
    digitalWrite(MOTOR2_STEP_PIN, LOW);

    stepCpt++;

    if (stepCpt == nbStepToCount) {
      changeState(DECELERATION);
      stateTimer = millis();
    } else if (stepCpt == nbStepForALap && state == DECELERATION) {
      if (move2 == FORWARD) {
        reverseGear(true);
      } else {
        lapsNumber++;
        initBeforeStartingANewLap();
      }

      changeState(ACCELERATION);
      stateTimer = millis();
    }
  }

  if ((millis() - stateTimer) >= ACCELERATION_FREQUENCY) {
    if (state == ACCELERATION) {
      currentSpeed += rpmAcceleration;
      computeStepperFrequency();

      if (currentSpeed >= speed) {
        changeState(CRUISE);
      }
    } else if (state == DECELERATION) {
      currentSpeed -= rpmAcceleration;

      if (currentSpeed < 1) {
        currentSpeed = 1;
      }

      computeStepperFrequency();
    }

    stateTimer = millis();
  }
}

inline void initBeforeStartingANewLap() {
  reverseGear(false);
  nbStepForALap = lapDistance * STEP_DIST_RATIO;
  rpmAcceleration = ACCELERATION_SPEED * accelerationPercent / 100.0;
  changeState(ACCELERATION);
}

void changeState(State_enum newState) {
  switch (newState) {
    case ACCELERATION:
      stepCpt = 0;
      currentSpeed = 1.0;
      computeStepperFrequency();
      nbStepToCount = stepCpt + nbStepForALap / 2;
      state = ACCELERATION;
      //Serial.println("ACCELERATION");
      break;
    case CRUISE:
      currentSpeed = speed;
      computeStepperFrequency();
      nbStepToCount = stepCpt + nbStepForALap - (stepCpt * 2);
      state = CRUISE;
      //Serial.println("CRUISE");
      break;
    case DECELERATION:
      state = DECELERATION;
      //Serial.println("DECELERATION");
      break;
  }
}

inline void computeStepperFrequency() {
  stepperFrequency = 60000000 / (currentSpeed * MICROSTEP);
}

void initMotors() {
  driver.beginSerial(115200);
  driver.push();        // Resets the register to default
  driver.pdn_disable(true);   // Use PDN/UART pin for communication
  driver.I_scale_analog(false); // Use internal voltage reference
  driver.rms_current(400);   // Set driver current to 400mA
  driver.mstep_reg_select(1);  // Microstep resolution selected by MSTEP register
  driver.microsteps(2);     // Set number of microsteps
  driver.TPWMTHRS(45);     // When the velocity exceeds the limit set by TPWMTHRS, the driver switches to spreadCycle
  driver.toff(2);        // Enable driver in software
}

void stealthChop2Autotune() {
  delay(200);

  unsigned long rpm = 70;
  unsigned long timeToWait = 60000000 / (rpm * MICROSTEP);

  // 300 step at velocity between 60-300 RPM
  for (int i = 0; i < 300; i++) {
    digitalWrite(MOTOR1_STEP_PIN, HIGH);
    digitalWrite(MOTOR2_STEP_PIN, HIGH);
    digitalWrite(MOTOR1_STEP_PIN, LOW);
    digitalWrite(MOTOR2_STEP_PIN, LOW);
    delayMicroseconds(timeToWait);
  }
}

void sleepMotors() {
  digitalWrite(MOTOR1_SLP_PIN, HIGH);
  digitalWrite(MOTOR2_SLP_PIN, LOW);
}

void wakeUpMotors() {
  digitalWrite(MOTOR1_SLP_PIN, LOW);
  digitalWrite(MOTOR2_SLP_PIN, HIGH);
  delay(200);
}

void reverseGear(bool enable) {
  if (enable) {
    digitalWrite(MOTOR1_DIR_PIN, LOW);
    digitalWrite(MOTOR2_DIR_PIN, LOW);
    move2 = BACKWARD;
  } else {
    digitalWrite(MOTOR1_DIR_PIN, HIGH);
    digitalWrite(MOTOR2_DIR_PIN, HIGH);
    move2 = FORWARD;
  }
}

void measureStepDistanceRatio() {
  unsigned long rpm = 60;
  unsigned long timeToWait = 60000000 / (rpm * MICROSTEP);

  for (int i = 0; i < 1000; i++) {
    digitalWrite(MOTOR1_STEP_PIN, HIGH);
    digitalWrite(MOTOR2_STEP_PIN, HIGH);
    digitalWrite(MOTOR1_STEP_PIN, LOW);
    digitalWrite(MOTOR2_STEP_PIN, LOW);
    delayMicroseconds(timeToWait);
  }
}

void saveDistance(float value) {
  byte *b = (byte *)&value;

  for (byte i = 0; i < sizeof(value); i++) {
    EEPROM.update(EEPROM_DISTANCE + i, b[i]);
  }
}

float getDistance() {
  float value = 0.0;
  byte *b = (byte *)&value;

  for (byte i = 0; i < sizeof(value); i++) {
    *b++ = EEPROM.read(EEPROM_DISTANCE + i);
  }

  return value;
}

void saveSpeed(float value) {
  byte *b = (byte *)&value;

  for (byte i = 0; i < sizeof(value); i++) {
    EEPROM.update(EEPROM_SPEED + i, b[i]);
  }
}

float getSpeed() {
  float value = 0.0;
  byte *b = (byte *)&value;

  for (byte i = 0; i < sizeof(value); i++) {
    *b++ = EEPROM.read(EEPROM_SPEED + i);
  }

  return value;
}

void saveAcceleration(byte value) {
  EEPROM.update(EEPROM_ACCELERATION, value);
}

byte getAcceleration() {
  byte result = EEPROM.read(EEPROM_ACCELERATION);

  return result;
}

void saveTimer(float value) {
  byte *b = (byte *)&value;

  for (byte i = 0; i < sizeof(value); i++) {
    EEPROM.update(EEPROM_TIMER + i, b[i]);
  }
}

float getTimer() {
  float value = 0.0;
  byte *b = (byte *)&value;

  for (byte i = 0; i < sizeof(value); i++) {
    *b++ = EEPROM.read(EEPROM_TIMER + i);
  }

  return value;
}

String parseCommand(String data, char separator, int index) {
  int found = 0;
  int strIndex[] = {0, -1};
  int maxIndex = data.length() - 1;

  for (int i = 0; i <= maxIndex && found <= index; i++) {
    if (data.charAt(i) == separator || i == maxIndex || data.charAt(i) == '\n' || data.charAt(i) == '\r') {
      found++;
      strIndex[0] = strIndex[1] + 1;
      strIndex[1] = (i == maxIndex) ? i + 1 : i;
    }
  }

  return found > index ? data.substring(strIndex[0], strIndex[1]) : "";
}
