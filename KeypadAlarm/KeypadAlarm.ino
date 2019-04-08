// Enable debug prints to serial monitor
#define MY_DEBUG

// Enable and select radio type attached
#define MY_RADIO_RF24

// Enable IRQ pin
#define MY_RX_MESSAGE_BUFFER_FEATURE
#define MY_RF24_IRQ_PIN (2)
#define MY_RX_MESSAGE_BUFFER_SIZE (10)

// RF24 PA level
#define MY_RF24_PA_LEVEL (RF24_PA_MAX)

// Enable hardware signing
#define MY_SIGNING_ATSHA204
#define MY_SIGNING_REQUEST_SIGNATURES

// Define pins
#define BUZZER 5
#define GREEN_LED A4
#define RED_LED A5

// Debug print
#if defined(MY_DEBUG)
#define DEBUG_PRINT(str) Serial.println(str);
#else
#define DEBUG_PRINT(str)
#endif

// Includes
#include <MySensors.h>
#include <Keypad.h>

// Keypad

char* _password = "0000";
int _keyboardPosition = 0;
unsigned long _keyboardInterruptTime = 0;
const byte _rows = 4;
const byte _cols = 3;
char _keys[_rows][_cols] = {
  {'1', '2', '3'},
  {'4', '5', '6'},
  {'7', '8', '9'},
  {'*', '0', '#'}
};
byte _rowPins[_rows] = {8, 7, 4, 3}; //connect to the row pinouts of the keypad
byte _colPins[_cols] = {A0, A1, A2}; //connect to the column pinouts of the keypad
Keypad _keypad = Keypad(makeKeymap(_keys), _rowPins, _colPins, _rows, _cols);

// Msg
MyMessage msg(0, V_CUSTOM);

void before()
{
  pinMode(BUZZER, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);
  pinMode(RED_LED, OUTPUT);
  digitalWrite(BUZZER, LOW);
  digitalWrite(GREEN_LED, HIGH);
  digitalWrite(RED_LED, LOW);
}

void setup() {
  initPasswordValue();
  send(msg.set(F("system started")));
  digitalWrite(GREEN_LED, LOW);
  for (byte i = 0; i < 10; i++) {
    if (i % 2 == 0) {
      digitalWrite(GREEN_LED, HIGH);
      digitalWrite(RED_LED, LOW);
    } else {
      digitalWrite(GREEN_LED, LOW);
      digitalWrite(RED_LED, HIGH);
    }
    delay(200);
  }
  digitalWrite(GREEN_LED, LOW);
  digitalWrite(RED_LED, LOW);
}

void presentation() {
  sendSketchInfo("Keypad Siren", "1.0");
  present(0, S_CUSTOM);
}

void loop() {
  char key = _keypad.getKey();

  // key pressed
  if (key != NO_KEY) {
    DEBUG_PRINT(F("Key pressed:"));
    DEBUG_PRINT(key);

    if (key == _password[_keyboardPosition]) {
      _keyboardPosition++;
    } else {
      _keyboardPosition = 0;
    }

    if (_keyboardPosition == 4) {
      DEBUG_PRINT(F("Correct password entered!"));
      digitalWrite(GREEN_LED, HIGH);
      buzz(200);
      digitalWrite(GREEN_LED, LOW);
      send(msg.set(F("password")));
      _keyboardPosition = 1;
      _keyboardInterruptTime = millis() - 60000;
    }
  }

  if (millis() - _keyboardInterruptTime >= 10000) {
    // Sleep until an key is pressed
    putKeypadToInterruptMode();
    sleep(digitalPinToInterrupt(_rowPins[_rows - 1]), FALLING, 0);
    _keyboardPosition = 1;
    _keyboardInterruptTime = millis();
  } else {
    wait(10);
  }
}

void buzz(int duration) {
  digitalWrite(BUZZER, HIGH);
  sleep(duration);
  digitalWrite(BUZZER, LOW);
}

void putKeypadToInterruptMode() {
  for (char i = 0; i < _cols; i++) {
    pinMode(_colPins[i], OUTPUT);
    digitalWrite(_colPins[i], LOW);
  }

  pinMode(_rowPins[_rows - 1], INPUT_PULLUP);
}

void initPasswordValue() {
  if (isAscii(loadState(0))) {
    for (char i = 0; i < strlen(_password); i++) {
      _password[i] = loadState(i);
    }
  }

  DEBUG_PRINT(F("Password is:"));
  DEBUG_PRINT(_password);
}

void savePassword(const char* password) {
  for (char i = 0; i < strlen(_password); i++) {
    _password[i] = password[i];
    saveState(i, _password[i]);
  }

  DEBUG_PRINT(F("New password saved:"));
  DEBUG_PRINT(_password);
}
