#define MICROSTEP 200 * 2
 
#define MOTOR1_DIR_PIN 8
#define MOTOR1_STEP_PIN 4
#define MOTOR1_SLP_PIN 9
#define MOTOR1_ENA_PIN A2

#define MOTOR2_DIR_PIN 11
#define MOTOR2_STEP_PIN 10
#define MOTOR2_SLP_PIN 12

unsigned long previousTime = micros();


unsigned long stepTimer = millis();
byte stepNb = 0;
byte direction = HIGH;

unsigned long stepperFrequency;
unsigned long rpmCruise = 260; // to define
unsigned long accelerationTime = 2000; // to define
unsigned long cruiseTime = 500;  // to define
float rpmAcceleration;
float currentRpm;
 
void setup()
{  
    //pinMode(MOTOR1_SLP_PIN, OUTPUT);
    pinMode(MOTOR1_SLP_PIN, INPUT);
    pinMode(MOTOR1_ENA_PIN, OUTPUT);
    pinMode(MOTOR2_SLP_PIN, OUTPUT);

    pinMode(MOTOR1_DIR_PIN, OUTPUT);
    pinMode(MOTOR2_DIR_PIN, OUTPUT);

    pinMode(MOTOR1_STEP_PIN, OUTPUT);
    pinMode(MOTOR2_STEP_PIN, OUTPUT);

    Serial.begin(115200);

    wakeUpMotors();

    digitalWrite(MOTOR1_DIR_PIN,HIGH);
    //digitalWrite(MOTOR2_DIR_PIN,LOW);

    stepNb = 0;
    stepTimer = millis();

    stealthChop2Autotune();
    delay(1000);

    currentRpm = 1.0;
    computeStepperFrequency();
    computeRpmAcceleration();
    Serial.print("rpm acceleration: ");
    Serial.println(rpmAcceleration);
}
 
void loop()
{
   manageAcceleration();
}

inline void manageAcceleration() {
    unsigned long currentTime = micros();
    if (currentTime - previousTime >= stepperFrequency) {
      previousTime = currentTime;

      digitalWrite(MOTOR1_STEP_PIN,HIGH);
      digitalWrite(MOTOR2_STEP_PIN,HIGH);
      digitalWrite(MOTOR1_STEP_PIN,LOW);
      digitalWrite(MOTOR2_STEP_PIN,LOW);
    }

    if (stepNb == 0) {
      if ((millis() - stepTimer) >= 50) {
        currentRpm += rpmAcceleration;
        computeStepperFrequency();

        if (currentRpm >= rpmCruise) {
          stepNb++;
        }
        
        stepTimer = millis();
      }
    } else if (stepNb == 1) {
      if ((millis() - stepTimer) >= cruiseTime) {
        stepNb++;
      }
    } else if (stepNb == 2) {
      if ((millis() - stepTimer) >= 50) {
        currentRpm -= rpmAcceleration;
        computeStepperFrequency();

        if (currentRpm < 5.0) {
          direction = direction == HIGH ? LOW : HIGH;
          digitalWrite(MOTOR1_DIR_PIN, direction);
          stepNb = 0;
        }
        
        stepTimer = millis();
      }
    }
}

inline void computeStepperFrequency() {
  stepperFrequency = 60000000 / (currentRpm * MICROSTEP);
}

inline void computeRpmAcceleration() {
  rpmAcceleration = (rpmCruise - 1.0) / (accelerationTime / 100.0);
}

void stealthChop2Autotune() {
  delay(200);

  unsigned long rpm = 70;
  unsigned long timeToWait = 60000000 / (rpm * MICROSTEP);

  // 300 step at velocity between 60-300 RPM
  for (int i=0; i<300; i++) {
    digitalWrite(MOTOR1_STEP_PIN,HIGH);
    digitalWrite(MOTOR2_STEP_PIN,HIGH);
    digitalWrite(MOTOR1_STEP_PIN,LOW);
    digitalWrite(MOTOR2_STEP_PIN,LOW);
    delayMicroseconds(timeToWait);
  }
}

void sleepMotors() {
  //digitalWrite(MOTOR1_SLP_PIN, LOW);
  digitalWrite(MOTOR2_SLP_PIN, LOW);
}

void wakeUpMotors() {
  //digitalWrite(MOTOR1_SLP_PIN, HIGH);
  digitalWrite(MOTOR1_ENA_PIN, LOW);
  digitalWrite(MOTOR2_SLP_PIN, HIGH);
  delay(1);
}
