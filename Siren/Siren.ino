// Enable debug prints to serial monitor
#define MY_DEBUG

// Enable and select radio type attached
#define MY_RADIO_RF24

// Enable IRQ pin
#define MY_RX_MESSAGE_BUFFER_FEATURE
#define MY_RF24_IRQ_PIN (2)

// RF24 PA level
#define MY_RF24_PA_LEVEL (RF24_PA_MAX)

// Enable repeater functionality
#define MY_REPEATER_FEATURE

// Define pins
#define SIREN 5
#define BUZZER 6
#define BLUE_LED A3
#define GREEN_LED A4
#define RED_LED A5
#define POWER_PROBE A6

// Debug print
#if defined(MY_DEBUG)
  #define DEBUG_PRINT(str) Serial.println(str);
#else
  #define DEBUG_PRINT(str)
#endif

// Includes
#include <MySensors.h>
#include <Keypad.h>
#include <RGBLed.h>  // https://github.com/wilmouths/RGBLed

// Keypad
char* _password = "0000";
int _keyboardPosition = 0;
bool _keyboardEnable = false;
unsigned long _keyboardEnableTime = 0;
const byte _rows = 4;
const byte _cols = 3;
char _keys[_rows][_cols] = {
  {'1','2','3'},
  {'4','5','6'},
  {'7','8','9'},
  {'*','0','#'}
};
byte _rowPins[_rows] = {8, 7, 4, 3}; //connect to the row pinouts of the keypad
byte _colPins[_cols] = {A0, A1, A2}; //connect to the column pinouts of the keypad
Keypad _keypad = Keypad(makeKeymap(_keys), _rowPins, _colPins, _rows, _cols);

// Others
MyMessage msg(0, V_CUSTOM);
unsigned long _sirenTime = 0;
int _sirenState = 0;
int _bipNumber = 0;
int _sirenLevel = 100;
bool _isOnBattery = false;
RGBLed led(RED_LED, GREEN_LED, BLUE_LED, COMMON_CATHODE);

void before()
{
  pinMode(SIREN, OUTPUT);
  digitalWrite(SIREN, LOW);
  pinMode(BUZZER, OUTPUT);
  digitalWrite(BUZZER, LOW);
  pinMode(POWER_PROBE, INPUT);

  stopSiren();
}

void setup() {
  attachInterrupt(digitalPinToInterrupt(_rowPins[_rows-1]), keyboardInterrupt, FALLING);
  initPasswordValue();
  led.flash(RGBLed::WHITE, 500);
  send(msg.set(F("system started")));
}

void presentation() {
  sendSketchInfo("Siren", "1.0");
  present(0, S_CUSTOM);
}

void receive(const MyMessage &myMsg)
{
  if (myMsg.type == V_CUSTOM) {
    String message = myMsg.getString();
    String arg0 = parseMsg(message, '-', 0);
    String arg1 = parseMsg(message, '-', 1);
    String arg2 = parseMsg(message, '-', 2);

    if (arg0 == "ping") {
      send(msg.set(F("pong")));
    } else if (arg0 == "stop") {
      stopSiren();
      send(msg.set(F("stopped by user")));
    } else if (arg0 == "start") {
      startSiren(arg1.toInt(), arg2.toInt());
      send(msg.set(F("started")));
    } else if (arg0 == "password") {
      if (arg1.length() != 4) {
        send(msg.set(F("password shall be on 4 c")));
      } else {
        savePassword(arg1);
        send(msg.set(F("password saved")));
      }
    } else {
      send(msg.set(F("args error")));
    }
  }
}

void loop() {
  manageKeyboard();
  manageSiren();
  managePowerProbe();
  wait(100);
}

void correctPasswordEntered() {
  if (isSirenOn()) {
    stopSiren();
    send(msg.set(F("stopped by password")));
  } else {
    send(msg.set(F("password entered")));
  }
}

void managePowerProbe() {
  boolean sectorOn = (analogRead(POWER_PROBE) > 900) ? true : false;
  
  if (!sectorOn && !_isOnBattery) {
    DEBUG_PRINT(F("On battery"));
    send(msg.set(F("on battery")));
    _isOnBattery = true;
    led.setColor(RGBLed::RED);
  } else if (sectorOn && _isOnBattery) {
    DEBUG_PRINT(F("On power supply"));
    send(msg.set(F("on power supply")));
    _isOnBattery = false;
    led.off();
  }
}

void startSiren(char bipBumber, char level) {
  _bipNumber = bipBumber;
  _sirenLevel = level;
  _sirenTime = 0;
  _sirenState = 1;
  DEBUG_PRINT(F("Siren started!"));
}

void stopSiren() {
  _sirenState = 0;
  stopSirenSound();
  DEBUG_PRINT(F("Siren stopped!"));
}

bool isSirenOn() {
  return _sirenState != 0;
}

void manageSiren() {
  if (_sirenState == 0) {
    // nothing to do
  } else if ((_sirenState % 2 == 1 && _sirenState < _bipNumber * 2 + 2) && millis() - _sirenTime >= 1000) {
    startSirenSound(_sirenLevel);
    _sirenTime = millis();
    _sirenState++;
  } else if ((_sirenState % 2 == 0 && _sirenState < _bipNumber * 2 + 2) && millis() - _sirenTime >= 100) {
    stopSirenSound();
    _sirenTime = millis();
    _sirenState++;
  } else if ((_sirenState == _bipNumber * 2 + 2) && millis() - _sirenTime >= 60000) {
    DEBUG_PRINT(F("Siren stopped after 60s!"));
    stopSiren();
    send(msg.set(F("stopped")));
  }
}

void manageKeyboard() {
    if (_keyboardEnable) {
      char key = _keypad.getKey();

      // key pressed
      if (key != NO_KEY) {
        DEBUG_PRINT(F("Key pressed:"));
        DEBUG_PRINT(key);
        buzz(50);
        
        if (key == _password[_keyboardPosition]) {
          _keyboardPosition++;
        } else {
          _keyboardPosition = 0;
        }

        if (_keyboardPosition == 4) {
          DEBUG_PRINT(F("Correct password entered!"));
          _keyboardPosition = 0;
          led.flash(RGBLed::GREEN, 100);
          buzz(200);
          correctPasswordEntered();
        }
      }
      
      if (millis() - _keyboardEnableTime >= 30000) {
        _keyboardEnable = false;
        _keyboardPosition = 0;
        putKeypadToInterruptMode();
        DEBUG_PRINT(F("Keyboard set to interrupt mode!"));
      }
  }
}

void startSirenSound(char level) {
    analogWrite(SIREN, map(level, 0, 100, 0, 255));
}

void stopSirenSound() {
    analogWrite(SIREN, 0);
}

void buzz(int timer) {
    analogWrite(BUZZER, 255);
    wait(timer);
    analogWrite(BUZZER, 0);
}

void putKeypadToInterruptMode() {
  for(char i=0; i<_cols; i++) {
    pinMode(_colPins[i], OUTPUT);
    digitalWrite(_colPins[i], LOW);
  }

  pinMode(_rowPins[_rows-1], INPUT_PULLUP);
}

void initPasswordValue() {
  if (isAscii(loadState(0))) {
      for (char i=0; i<strlen(_password); i++) {
        _password[i] = loadState(i);
      }
  }
  
  DEBUG_PRINT(F("Password is:"));
  DEBUG_PRINT(_password);
}

void savePassword(String password) {
    for (char i=0; i<strlen(_password); i++) {
        _password[i] = password.charAt(i);
        saveState(i, _password[i]);
    }
    
  DEBUG_PRINT(F("New password saved:"));
  DEBUG_PRINT(_password);
}

void keyboardInterrupt() {
  _keyboardEnable = true;
  _keyboardEnableTime = millis();
}

String parseMsg(String data, char separator, int index) {
  int found = 0;
  int strIndex[] = {0, -1};
  int maxIndex = data.length() - 1;

  for (int i = 0; i <= maxIndex && found <= index; i++) {
    if (data.charAt(i) == separator || i == maxIndex) {
      found++;
      strIndex[0] = strIndex[1] + 1;
      strIndex[1] = (i == maxIndex) ? i + 1 : i;
    }
  }

  return found > index ? data.substring(strIndex[0], strIndex[1]) : "";
}
