#define BUTTON 3
#define CMD 4
#define LED1 5
#define LED2 6
#define LED3 8
#define LED4 7
#define LEVEL A0

float _voltageCorrection = 1.0;

enum State_enum {FULL, ALMOST, HALF, LOWER};
uint8_t _state;

void setup() {
  pinMode(BUTTON, INPUT_PULLUP);
  pinMode(LED1, OUTPUT);
  pinMode(LED2, OUTPUT);
  pinMode(LED3, OUTPUT);
  pinMode(LED4, OUTPUT);
  pinMode(CMD, OUTPUT);
  pinMode(LEVEL, INPUT);

  digitalWrite(CMD, HIGH);

  //Serial.begin(115200);
  changeState(FULL);
}

void loop() {
  float volt = getVoltage();
  
  if (_state == FULL && volt < 4.00 * 4.0) {
    changeState(ALMOST);
  } else if (_state == ALMOST && volt < 3.85 * 4.0) {
    changeState(HALF);
  } else if (_state == HALF && volt < 3.70 * 4.0) {
    changeState(LOWER);
  } else if (_state == LOWER && volt < 3.50 * 4.0) {
    digitalWrite(CMD, LOW);
  }

  if (digitalRead(BUTTON) == LOW) {
    digitalWrite(CMD, LOW);
  }

  delay(500);
}

void changeState(uint8_t state) {
  switch (state) {
    case FULL:
      digitalWrite(LED1, HIGH);
      digitalWrite(LED2, HIGH);
      digitalWrite(LED3, HIGH);
      digitalWrite(LED4, HIGH);
      break;
    case ALMOST:
      digitalWrite(LED1, HIGH);
      digitalWrite(LED2, HIGH);
      digitalWrite(LED3, HIGH);
      digitalWrite(LED4, LOW);
      break;
    case HALF:
      digitalWrite(LED1, HIGH);
      digitalWrite(LED2, HIGH);
      digitalWrite(LED3, LOW);
      digitalWrite(LED4, LOW);
      break;
    case LOWER:
      digitalWrite(LED1, HIGH);
      digitalWrite(LED2, LOW);
      digitalWrite(LED3, LOW);
      digitalWrite(LED4, LOW);
  }

  _state = state;
}

float getVoltage() {
  int analog = analogRead(LEVEL);
  float u2 = (analog * 3.3) / 1023.0;
  float voltage = (u2 * (43000.0 + 10000.0)) / 10000.0;
  voltage *= _voltageCorrection;

  return voltage;
}
