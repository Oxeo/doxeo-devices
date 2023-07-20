#include "Arduino.h"
#include <EEPROM.h>
#include <TMC2208Stepper.h>
#include <SoftwareSerial.h>

#define STEPMOTOR 200
#define MICROSTEP 2
#define STEP_DIST_RATIO (500 * MICROSTEP) / 15                     // step numbers divided by robot distance (cm)
#define ACCELERATION_FREQUENCY 10.0                   // milliseconds
#define ACCELERATION_SPEED (500.0 / (1000.0 / ACCELERATION_FREQUENCY)) // 500 RPM of acceleration in 1 second

#define MOTOR1_DIR_PIN 8
#define MOTOR1_STEP_PIN 4
#define MOTOR1_SLP_PIN 9
#define MOTOR1_UART_PIN A2

#define MOTOR2_DIR_PIN 11
#define MOTOR2_STEP_PIN 10
#define MOTOR2_SLP_PIN 12
#define MOTOR2_UART_PIN A3

#define BLE_RX_PIN 5
#define BLE_TX_PIN 6
#define BLE_LINK_PIN 7
#define BLE_DATA_PIN 3

#define BUTTON_1 2

#define EEPROM_DISTANCE 0
#define EEPROM_SPEED 4
#define EEPROM_TIMER 8
#define EEPROM_ACCELERATION 12
#define EEPROM_MOTOR_CURRENT 16

enum State_enum {FIX, ACCELERATION, CRUISE, DECELERATION};
enum MoveStatus_enum {FORWARD, BACKWARD};

TMC2208Stepper driver1 = TMC2208Stepper(-1, MOTOR1_UART_PIN, false);
TMC2208Stepper driver2 = TMC2208Stepper(-1, MOTOR2_UART_PIN, false);
SoftwareSerial ble(BLE_TX_PIN, BLE_RX_PIN); // RX, TX

int travelSpeedRpm;
int travelDistanceCm;
int travelTimeMinute;
byte accelerationPercent;
unsigned long previousTime = micros();
unsigned long stateTimer = millis();
unsigned long stepperFrequency;
float accelerationRpm;
float currentSpeedRpm;
unsigned long stepCpt;
unsigned long nbStepToDestination;
unsigned long stepNumberToActivateDeceleration;
uint8_t state = FIX;
uint8_t moveStatus;
int cruiseSpeedRpm;
bool run;
int motorCurrent;
unsigned long startTimer;

void setup()
{
  Serial.begin(9600);
  ble.begin(9600);

  travelDistanceCm = getDistance();
  travelSpeedRpm = getSpeed();
  accelerationPercent = getAcceleration();
  travelTimeMinute = getTimer();
  motorCurrent = getMotorCurrent();

  initMotors();

  pinMode(BLE_LINK_PIN, INPUT);
  pinMode(BLE_DATA_PIN, INPUT);
  pinMode(MOTOR1_SLP_PIN, OUTPUT);
  pinMode(MOTOR2_SLP_PIN, OUTPUT);
  pinMode(MOTOR1_DIR_PIN, OUTPUT);
  pinMode(MOTOR2_DIR_PIN, OUTPUT);
  pinMode(MOTOR1_STEP_PIN, OUTPUT);
  pinMode(MOTOR2_STEP_PIN, OUTPUT);
  pinMode(BUTTON_1, INPUT);

  sleepMotors();

  if (digitalRead(BLE_LINK_PIN) == HIGH) {
    sendDataToBleDevice();
  }

  Serial.println("Initialisation done");
  //measureStepDistanceRatio();
}

void loop()
{
  if (state != FIX) {
    moveRobot();

    if (state == FIX) {
      if (moveStatus == FORWARD) {
        changeMoveStatus(BACKWARD);
        changeState(ACCELERATION);
      } else {
        if (run) {
          startRobot();
        } else {
          sleepMotors();
        }
      }
    }

    if ((millis() - startTimer) >= (unsigned long) travelTimeMinute * 60000UL) {
      run = false;
    }
  }

  if (digitalRead(BUTTON_1) == LOW) {
    if (run) {
      urgentStop();
    } else {
      delay(500);
      unsigned long waitTime = millis();
      while (digitalRead(BUTTON_1) == LOW);

      if ((millis() - waitTime) >= 1000) {
        start();
      }
    }

    delay(500);
  }

  manageBleMessage();
}

void start() {
  wakeUpMotors();
  startRobot();
  startTimer = millis();

  if (digitalRead(BLE_LINK_PIN) == HIGH) {
    ble.println("status:on");
  }
}

void stop() {
  stopRobot();

  if (digitalRead(BLE_LINK_PIN) == HIGH) {
    ble.println("status:off");
  }
}

void urgentStop() {
  stopRobot();
  changeState(FIX);
  sleepMotors();

  if (digitalRead(BLE_LINK_PIN) == HIGH) {
    ble.println("status:off");
  }
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

    if (stepCpt == stepNumberToActivateDeceleration) {
      changeState(DECELERATION);
    } else if (stepCpt == nbStepToDestination) {
      changeState(FIX);
    }
  }

  if ((millis() - stateTimer) >= ACCELERATION_FREQUENCY) {
    if (state == ACCELERATION) {
      currentSpeedRpm += accelerationRpm;
      computeStepperFrequency();

      if (currentSpeedRpm >= cruiseSpeedRpm) {
        changeState(CRUISE);
      }
    } else if (state == DECELERATION) {
      currentSpeedRpm -= accelerationRpm;

      if (currentSpeedRpm < 1.0) {
        currentSpeedRpm = 1.0;
      }

      computeStepperFrequency();
    }

    stateTimer = millis();
  }
}

void startRobot() {
  changeMoveStatus(FORWARD);
  cruiseSpeedRpm = travelSpeedRpm;
  nbStepToDestination = (float) travelDistanceCm * STEP_DIST_RATIO;
  accelerationRpm = ACCELERATION_SPEED * accelerationPercent / 100.0;
  changeState(ACCELERATION);
  run = true;
}

void stopRobot() {
  run = false;
}

void changeState(State_enum newState) {
  switch (newState) {
    case ACCELERATION:
      stepCpt = 0;
      currentSpeedRpm = 1.0;
      computeStepperFrequency();
      stepNumberToActivateDeceleration = nbStepToDestination / 2;
      state = ACCELERATION;
      stateTimer = millis();
      //Serial.println("ACCELERATION");
      break;
    case CRUISE:
      currentSpeedRpm = cruiseSpeedRpm;
      computeStepperFrequency();
      stepNumberToActivateDeceleration = nbStepToDestination - stepCpt;
      state = CRUISE;
      stateTimer = millis();
      //Serial.println("CRUISE");
      break;
    case DECELERATION:
      state = DECELERATION;
      stateTimer = millis();
      //Serial.println("DECELERATION");
      break;
    case FIX:
      state = FIX;
      //Serial.println("FIX");
      break;
  }
}

inline void computeStepperFrequency() {
  stepperFrequency = 60000000 / (currentSpeedRpm * MICROSTEP * STEPMOTOR);
}

inline void manageBleMessage() {
  //if (digitalRead(BLE_DATA_PIN) == HIGH) {
    while (ble.available()) {
      String msg = ble.readStringUntil('\n');
      Serial.println("ble: " + msg);

      if (msg == "cmd+start") {
        start();
      } else if (msg == "cmd+stop") {
        //stop();
        urgentStop();
      } else if (msg == "cmd+brake") {
        urgentStop();
      } else if (msg.startsWith("cmd+dist=")) {
        int distance = parseCommand(msg, '=', 1).toInt();

        if (distance > 0 && distance <= 100) {
          saveDistance(distance);
          ble.println("dist:" + String(distance));
        }
      } else if (msg.startsWith("cmd+speed=")) {
        int speed = parseCommand(msg, '=', 1).toInt();

        if (speed > 0 && speed <= 600) {
          saveSpeed(speed);
          ble.println("speed:" + String(speed));
        }
      } else if (msg.startsWith("cmd+accel=")) {
        int acceleration = parseCommand(msg, '=', 1).toInt();

        if (acceleration > 0 && acceleration <= 100) {
          saveAcceleration(acceleration);
          ble.println("accel:" + String(acceleration));
        }
      } else if (msg.startsWith("cmd+timer=")) {
        int timer = parseCommand(msg, '=', 1).toInt();

        if (timer > 0 && timer <= 120) {
          saveTimer(timer);
          ble.println("timer:" + String(timer));
        }
      } else if (msg == "cmd+data?") {
        sendDataToBleDevice();
      } else if (msg.startsWith("cmd+current=")) {
        int current = parseCommand(msg, '=', 1).toInt();

        if (current >= 100 && current <= 1500) {
          saveMotorCurrent(current);
          driver1.rms_current(motorCurrent);
          driver2.rms_current(motorCurrent);
          ble.println("current:" + String(current));
        }
      }
    }
  //}

  if (Serial.available() > 0) {
    char incomingByte = Serial.read();
    ble.write(incomingByte);
  }
}

void initMotors() {
  driver1.beginSerial(115200);
  driver1.push();  // Resets the register to default
  driver1.pdn_disable(true); // Use PDN/UART pin for communication
  driver1.I_scale_analog(false); // Use internal voltage reference
  driver1.rms_current(motorCurrent); // Set driver current to 400mA
  driver1.mstep_reg_select(1); // Microstep resolution selected by MSTEP register
  driver1.microsteps(MICROSTEP);  // Set number of microsteps
  driver1.TPWMTHRS(45);  // When the velocity exceeds the limit set by TPWMTHRS, the driver switches to spreadCycle
  driver1.toff(2);  // Enable driver in software

  driver2.beginSerial(115200);
  driver2.push();
  driver2.pdn_disable(true);
  driver2.I_scale_analog(false);
  driver2.rms_current(motorCurrent);
  driver2.mstep_reg_select(1);
  driver2.microsteps(MICROSTEP);
  driver2.TPWMTHRS(45);
  driver2.toff(2);
}

void stealthChop2Autotune() {
  delay(200);

  unsigned long rpm = 70;
  unsigned long timeToWait = 60000000 / (rpm * MICROSTEP * STEPMOTOR);

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
  digitalWrite(MOTOR2_SLP_PIN, HIGH);
}

void wakeUpMotors() {
  digitalWrite(MOTOR1_SLP_PIN, LOW);
  digitalWrite(MOTOR2_SLP_PIN, LOW);
  delay(200);
}

void changeMoveStatus(MoveStatus_enum newStatus) {
  if (newStatus == BACKWARD) {
    digitalWrite(MOTOR1_DIR_PIN, LOW);
    digitalWrite(MOTOR2_DIR_PIN, HIGH);
    moveStatus = BACKWARD;
  } else {
    digitalWrite(MOTOR1_DIR_PIN, HIGH);
    digitalWrite(MOTOR2_DIR_PIN, LOW);
    moveStatus = FORWARD;
  }
}

void sendDataToBleDevice() {
  String status = run ? "on" : "off";

  ble.println("status:" + status);
  ble.println("dist:" + String(travelDistanceCm));
  ble.println("speed:" + String(travelSpeedRpm));
  ble.println("accel:" + String(accelerationPercent));
  ble.println("timer:" + String(travelTimeMinute));
  ble.println("current:" + String(motorCurrent));
}

void measureStepDistanceRatio() {
  unsigned long rpm = 60;
  unsigned long timeToWait = 60000000 / (rpm * MICROSTEP * STEPMOTOR);

  for (int i = 0; i < 1000; i++) {
    digitalWrite(MOTOR1_STEP_PIN, HIGH);
    digitalWrite(MOTOR2_STEP_PIN, HIGH);
    digitalWrite(MOTOR1_STEP_PIN, LOW);
    digitalWrite(MOTOR2_STEP_PIN, LOW);
    delayMicroseconds(timeToWait);
  }
}

void saveDistance(int value) {
  byte *b = (byte *)&value;

  for (byte i = 0; i < sizeof(value); i++) {
    EEPROM.update(EEPROM_DISTANCE + i, b[i]);
  }

  travelDistanceCm = value;
}

int getDistance() {
  int value = 0.0;
  byte *b = (byte *)&value;

  for (byte i = 0; i < sizeof(value); i++) {
    *b++ = EEPROM.read(EEPROM_DISTANCE + i);
  }

  return value;
}

void saveSpeed(int value) {
  byte *b = (byte *)&value;

  for (byte i = 0; i < sizeof(value); i++) {
    EEPROM.update(EEPROM_SPEED + i, b[i]);
  }

  travelSpeedRpm = value;
}

int getSpeed() {
  int value = 0.0;
  byte *b = (byte *)&value;

  for (byte i = 0; i < sizeof(value); i++) {
    *b++ = EEPROM.read(EEPROM_SPEED + i);
  }

  return value;
}

void saveAcceleration(byte value) {
  EEPROM.update(EEPROM_ACCELERATION, value);
  accelerationPercent = value;
}

byte getAcceleration() {
  byte result = EEPROM.read(EEPROM_ACCELERATION);

  return result;
}

void saveTimer(int value) {
  byte *b = (byte *)&value;

  for (byte i = 0; i < sizeof(value); i++) {
    EEPROM.update(EEPROM_TIMER + i, b[i]);
  }

  travelTimeMinute = value;
}

int getTimer() {
  int value = 0.0;
  byte *b = (byte *)&value;

  for (byte i = 0; i < sizeof(value); i++) {
    *b++ = EEPROM.read(EEPROM_TIMER + i);
  }

  return value;
}

void saveMotorCurrent(int value) {
  byte *b = (byte *)&value;

  for (byte i = 0; i < sizeof(value); i++) {
    EEPROM.update(EEPROM_MOTOR_CURRENT + i, b[i]);
  }

  motorCurrent = value;
}

int getMotorCurrent() {
  int value = 0.0;
  byte *b = (byte *)&value;

  for (byte i = 0; i < sizeof(value); i++) {
    *b++ = EEPROM.read(EEPROM_MOTOR_CURRENT + i);
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
