// Enable debug prints to serial monitor
#define MY_DEBUG

// Enable and select radio type attached
#define MY_RADIO_RF24

// Enable IRQ pin
#define MY_RX_MESSAGE_BUFFER_FEATURE
#define MY_RF24_IRQ_PIN (2)
#define MY_RX_MESSAGE_BUFFER_SIZE (5)

// RF24 PA level
#define MY_RF24_PA_LEVEL (RF24_PA_HIGH)

// Enable hardware signing
#define MY_SIGNING_ATSHA204
#define MY_SIGNING_REQUEST_SIGNATURES

// Define pins
#define BUZZER 5
#define GREEN_LED A4
#define RED_LED A5
#define BELL 6

// Debug print
#if defined(MY_DEBUG)
#define DEBUG_PRINT(str) Serial.println(str);
#else
#define DEBUG_PRINT(str)
#endif

// Includes
#include <MySensors.h>
#include <Keypad.h>
#include <Vcc.h>

// Battery report
static uint8_t _oldBatteryPcnt = 200;  // Initialize to 200 to assure first time value will be sent.
const float _vccMin        = 2.8;      // Minimum expected Vcc level, in Volts: Brownout at 2.8V    -> 0%
const float _vccMax        = 2.0*1.6;  // Maximum expected Vcc level, in Volts: 2xAA fresh Alkaline -> 100%
const float _vccCorrection = 1.0;      // Measured Vcc by multimeter divided by reported Vcc
static Vcc _vcc(_vccCorrection);

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
  {'0', '0', '#'}
};
byte _rowPins[_rows] = {8, 7, 4, 3}; //connect to the row pinouts of the keypad
byte _colPins[_cols] = {A0, A1, A2}; //connect to the column pinouts of the keypad
Keypad _keypad = Keypad(makeKeymap(_keys), _rowPins, _colPins, _rows, _cols);

// State
enum State_enum {NOMINAL, CHANGE_PASSWORD, CHECK_PASSWORD};
uint8_t _state = NOMINAL;

// Msg
MyMessage msg(0, V_CUSTOM);

void before()
{
  pinMode(BUZZER, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);
  pinMode(RED_LED, OUTPUT);
  pinMode(BELL, INPUT);
  digitalWrite(BUZZER, LOW);
  digitalWrite(GREEN_LED, LOW);
  digitalWrite(RED_LED, HIGH);
}

void setup() {
  initPasswordValue();
  send(msg.set(F("system started")));
  digitalWrite(RED_LED, LOW);
  for (byte i = 0; i < 10; i++) {
    if (i % 2 == 0) {
      digitalWrite(GREEN_LED, HIGH);
    } else {
      digitalWrite(GREEN_LED, LOW);
    }
    delay(200);
  }
  _keyboardInterruptTime = 0;
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
    _keyboardInterruptTime = millis();

    if (_state == NOMINAL) {
      if (_keyboardPosition == 1 && key == '#') {
        send(msg.set(F("#")));
      }
      
      if (key == _password[_keyboardPosition]) {
        _keyboardPosition++;
      } else if (key == _password[0]) {
        _keyboardPosition = 1;
      } else {
        _keyboardPosition = 0;
      }

      if (_keyboardPosition == 4) {
        _keyboardPosition = 0;

        if (digitalRead(BELL) == LOW) {
          DEBUG_PRINT(F("Correct password entered!"));
          digitalWrite(GREEN_LED, HIGH);
          send(msg.set(F("ok")));
          delay(200);
          digitalWrite(GREEN_LED, LOW);
          _keyboardInterruptTime -= 60000;
        } else {
          DEBUG_PRINT(F("change password"));
          _state = CHANGE_PASSWORD;
          digitalWrite(RED_LED, HIGH);
          digitalWrite(GREEN_LED, HIGH);
          buzz(200);
        }
      }
    } else if (_state == CHANGE_PASSWORD) {
      _password[_keyboardPosition] = key;
      _keyboardPosition++;

      if (_keyboardPosition == 4) {
        DEBUG_PRINT(F("check password"));
        _state = CHECK_PASSWORD;
        _keyboardPosition = 0;
        digitalWrite(RED_LED, LOW);
        delay(500);
        buzz(10);
        digitalWrite(RED_LED, HIGH);
      }
    } else if (_state == CHECK_PASSWORD) {
      if (key == _password[_keyboardPosition]) {
        _keyboardPosition++;
      } else {
        DEBUG_PRINT(F("change password failed"));
        initPasswordValue();
        _keyboardInterruptTime -= 60000;
      }

      // success
      if (_keyboardPosition == 4) {
        savePassword(_password);
        for (byte i = 0; i < 10; i++) {
          if (i % 2 == 0) {
            digitalWrite(GREEN_LED, LOW);
          } else {
            digitalWrite(GREEN_LED, HIGH);
            buzz(10);
          }
          delay(500);
        }
        _keyboardInterruptTime -= 60000;
      }
    }
  }

  if (millis() - _keyboardInterruptTime >= 10000) {
    // Sleep until key 0 is pressed
    _state = NOMINAL;
    digitalWrite(GREEN_LED, LOW);
    digitalWrite(RED_LED, LOW);
    reportBatteryLevel();
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

inline void reportBatteryLevel() {
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
}
