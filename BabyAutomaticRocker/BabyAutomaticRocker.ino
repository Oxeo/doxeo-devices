#include <TMC2208Stepper.h>

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

enum State_enum {FIX, ACCELERATION, CRUISE, DECELERATION};
enum move2_enum {FORWARD, BACKWARD};

TMC2208Stepper driver = TMC2208Stepper(-1, MOTOR1_UART_PIN, false);

// to define
unsigned long speed = 260;       // RPM
float accelerationPercent = 50.0;
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
  Serial.begin(115200);
  initMotors();

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
  if (lapsNumber < 2) {
    moveRobot();
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
