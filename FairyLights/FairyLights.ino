const int _tableLength = 7;
int _table[_tableLength];

void setup() {
  Serial.begin(115200);

  pinMode(3, OUTPUT);
  pinMode(4, OUTPUT);
  pinMode(5, OUTPUT);
  pinMode(6, OUTPUT);
  pinMode(7, OUTPUT);
  pinMode(8, OUTPUT);

  randomSeed(analogRead(0));

  lights(1, 0, 0, 0, 0, 0, 200);
  lights(1, 1, 0, 0, 0, 0, 200);
  lights(1, 1, 0, 1, 0, 0, 200);
  lights(1, 1, 0, 1, 1, 0, 200);
  lights(1, 1, 0, 1, 1, 1, 200);
  lights(1, 1, 1, 1, 1, 1, 5000);
  lights(1, 1, 0, 1, 1, 1, 200);
  lights(1, 1, 0, 1, 1, 0, 200);
  lights(1, 1, 0, 1, 0, 0, 200);
  lights(1, 1, 0, 0, 0, 0, 200);
  lights(1, 0, 0, 0, 0, 0, 200);
  lights(0, 0, 0, 0, 0, 0, 200);
}

void loop() {
  generateTable();
  printTable();

  for (int i = 0; i < _tableLength; i++) {
    animate(_table[i], secondes(60));
  }
}

void animate(byte step, unsigned long time) {
  unsigned long startTime = millis();
  int randomNb = -1;

  Serial.print("Step ");
  Serial.println(step);
  while (millis() - startTime < time) {
    switch (step) {
      case 1:
        lights(1, 0, 0, 1, 1, 1, 1000);
      case 2:
        lights(1, 0, 0, 1, 1, 0, 200);
        lights(1, 0, 0, 1, 0, 1, 200);
        lights(1, 0, 0, 0, 1, 1, 200);
        lights(1, 0, 0, 1, 1, 1, random(10000));
        break;
      case 3:
        lights(1, 1, 1, 1, 1, 1, 15000);
        lights(1, 1, 0, 1, 1, 1, 200);
        lights(1, 1, 0, 1, 1, 0, 200);
        lights(1, 1, 0, 1, 0, 0, 200);
        lights(1, 1, 0, 0, 0, 0, 200);
        lights(1, 0, 0, 0, 0, 0, 200);
        lights(0, 0, 0, 0, 0, 0, 5000);
        lights(1, 0, 0, 0, 0, 0, 200);
        lights(1, 1, 0, 0, 0, 0, 200);
        lights(1, 1, 0, 1, 0, 0, 200);
        lights(1, 1, 0, 1, 1, 0, 200);
        lights(1, 1, 0, 1, 1, 1, 200);
        break;
      case 4:
        randomNb = random(3);
        if (randomNb == 0) {
          lights(1, 0, 0, 0, 1, 1, 100);
        } else if (randomNb == 1) {
          lights(1, 0, 0, 1, 0, 1, 100);
        } else {
          lights(1, 0, 0, 1, 1, 0, 100);
        }
        lights(1, 0, 0, 1, 1, 1, random(1000, 5000));
        break;
      case 5:
        randomNb = random(3);
        if (randomNb == 0) {
          lights(0, 1, 1, 1, 1, 1, 100);
        } else if (randomNb == 1) {
          lights(1, 0, 1, 1, 1, 1, 100);
        } else {
          lights(1, 1, 0, 1, 1, 1, 100);
        }
        lights(1, 1, 1, 1, 1, 1, random(1000, 5000));
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

void generateTable() {
  for (int i = 0; i < _tableLength; i++) {
    _table[i] = -1;
  }

  for (int i = 0; i < _tableLength; i++) {
    int num;

    while (true) {
      num = random(_tableLength);

      bool found = false;
      for (int j = 0; j < _tableLength && _table[j] != -1; j++) {
        if (_table[j] == num) {
          found = true;
          break;
        }
      }

      if (!found) {
        break;
      }
    }

    _table[i] = num;
  }
}

void printTable() {
  Serial.print("Table=[");
  for (int i = 0; i < _tableLength; i++) {
    Serial.print(_table[i]);

    if (i < _tableLength - 1) {
      Serial.print(", ");
    }
  }
  Serial.println("]");
}

inline void lights(byte l1, byte l2, byte l3, byte l4, byte l5, byte l6, unsigned long timer) {
  if (l1) {
    Serial.print("(x)");
  } else {
    Serial.print("( )");
  }
  if (l2) {
    Serial.print(" (x)");
  } else {
    Serial.print(" ( )");
  }
  if (l3) {
    Serial.print(" (x)");
  } else {
    Serial.print(" ( )");
  }
  if (l4) {
    Serial.print(" (x)");
  } else {
    Serial.print(" ( )");
  }
  if (l5) {
    Serial.print(" (x)");
  } else {
    Serial.print(" ( )");
  }
  if (l6) {
    Serial.print(" (x) ");
  } else {
    Serial.print(" ( ) ");
  }
  Serial.println(timer);
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
