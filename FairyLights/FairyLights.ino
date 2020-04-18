byte _step = 0;
long _randomNumber = 0;

void setup() {
  Serial.begin(115200);

  pinMode(3, OUTPUT);
  pinMode(4, OUTPUT);
  pinMode(5, OUTPUT);
  pinMode(6, OUTPUT);
  pinMode(7, OUTPUT);
  pinMode(8, OUTPUT);

  randomSeed(analogRead(0));
}

void loop() {
  _step = getRandomNumber(5, _step);
  animate(_step, secondes(60));
}

void animate(byte step, unsigned long time) {
  unsigned long startTime = millis();

  Serial.print("Step ");
  Serial.println(step);
  while (millis() - startTime < time) {
    switch (step) {
      case 0:
        _randomNumber = getRandomNumber(4, _randomNumber);
        if (_randomNumber == 0) {
          lights(1, 0, 0, 0, 0, 0, 5000);
        } else if (_randomNumber == 1) {
          lights(0, 1, 0, 0, 0, 0, 5000);
        } else if (_randomNumber == 2) {
          lights(0, 0, 1, 0, 0, 0, 5000);
        } else if (_randomNumber == 3) {
          lights(0, 0, 0, 1, 1, 1, 5000);
        }
        break;
      case 1:
        _randomNumber = getRandomNumber(3, _randomNumber);
        if (_randomNumber == 0) {
          lights(1, 0, 0, 0, 1, 1, 5000);
        } else if (_randomNumber == 1) {
          lights(1, 0, 0, 1, 0, 1, 5000);
        } else {
          lights(1, 0, 0, 1, 1, 0, 5000);
        }
        break;
      case 2:
        lights(1, 0, 0, 1, 1, 0, 200);
        lights(1, 0, 0, 1, 0, 1, 200);
        lights(1, 0, 0, 0, 1, 1, 200);
        lights(1, 0, 0, 11, 1, 1, random(10000));
        break;
      case 3:
        lights(0, 1, 1, 0, 0, 0, 30000);
        lights(1, 0, 0, 0, 0, 0, 30000);
        break;
      case 4:
        lights(0, 1, 0, 1, 1, 1, 10000);
        lights(0, 0, 1, 1, 1, 1, 10000);
        break;
      default:
        lights(1, 1, 1, 1, 1, 1, 1000);
    }
  }
}

long getRandomNumber(long max, long forbidenValue) {
  long number = 0;

  do {
    number = random(max);
  } while (number == forbidenValue);

  return number;
}

inline void lights(byte l1, byte l2, byte l3, byte l4, byte l5, byte l6, unsigned long timer) {
  digitalWrite(3, (l1 > 0) ? HIGH : LOW);
  digitalWrite(4, (l2 > 0) ? HIGH : LOW);
  digitalWrite(5, (l3 > 0) ? HIGH : LOW);
  digitalWrite(6, (l4 > 0) ? HIGH : LOW);
  digitalWrite(7, (l5 > 0) ? HIGH : LOW);
  digitalWrite(8, (l6 > 0) ? HIGH : LOW);
  delay(timer);
}

inline unsigned long minutes(unsigned long minutes) {
  return minutes * 60000;
}

inline unsigned long secondes(unsigned long secondes) {
  return secondes * 1000;
}
