// Enable debug prints to serial monitor
#define MY_DEBUG

// Enable and select radio type attached
#define MY_RADIO_RF24

// Enable IRQ pin
#define MY_RX_MESSAGE_BUFFER_FEATURE
#define MY_RF24_IRQ_PIN (3)

// RF24 PA level
#define MY_RF24_PA_LEVEL (RF24_PA_LOW)

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
//#include <Keypad.h>
#include <RGBLed.h>  // https://github.com/wilmouths/RGBLed
#include <Parser.h>

// Keypad
/*char* _password = "0000";
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
*/

// Siren
unsigned long _sirenTime = 0;
int _sirenState = 0;
byte _bipNumber = 0;
byte _sirenLevel = 100;

// Buzzer
int _buzzerDuration = 0;
unsigned long _buzzerStartTime = 0;
bool _buzzerIsOn = false;

// Others
MyMessage msg(0, V_CUSTOM);
bool _isOnBattery = false;
RGBLed led(RED_LED, GREEN_LED, BLUE_LED, COMMON_CATHODE);
Parser parser = Parser(' ');
unsigned long _heartbeatTime = 0;

void before()
{
  pinMode(SIREN, OUTPUT);
  pinMode(BUZZER, OUTPUT);
  pinMode(POWER_PROBE, INPUT);

  stopSiren();
  stopBuzzer();
  led.setColor(RGBLed::RED);
}

void setup() {
  randomSeed(analogRead(0)); // A0
  //attachInterrupt(digitalPinToInterrupt(_rowPins[_rows-1]), keyboardInterrupt, FALLING);
  //initPasswordValue();
  led.flash(RGBLed::GREEN, 500);
  send(msg.set(F("system started")));
}

void presentation() {
  sendSketchInfo("Siren", "2.1");
  present(0, S_CUSTOM);
}

void receive(const MyMessage &myMsg)
{
  if (myMsg.type == V_CUSTOM && myMsg.sensor == 0) {
    parser.parse(myMsg.getString());

    if (parser.get(0) == NULL) {
      send(msg.set(F("cmd missing! send help")));
    } else if (parser.isEqual(0, "ping")) {
      // nothing to do
    } else if (parser.isEqual(0, "status")) {
      sendNrfStatus();
    } else if (parser.isEqual(0, "stop")) {
      stopSiren();
      send(msg.set(F("siren stopped by user")));
    } else if (parser.isEqual(0, "start") && parser.get(1) != NULL && parser.get(2) != NULL) {
      startSiren(parser.getInt(1), parser.getInt(2));
      send(msg.set(F("siren started")));
      /*    } else if (parser.isEqual(0, "password") && parser.get(1) != NULL) {
            if (strlen(parser.get(1)) != 4) {
              send(msg.set(F("password shall be on 4 c")));
            } else {
              savePassword(parser.get(1));
              send(msg.set(F("password saved")));
            }*/
    } else if (parser.isEqual(0, "buzzer") && parser.get(1) != NULL) {
      startBuzzer(parser.getInt(1));
      send(msg.set(F("buzzer started")));
    } else if (parser.isEqual(0, "help")) {
      send(msg.set(F("cmd1: ping")));
      send(msg.set(F("cmd2: stop")));
      send(msg.set(F("cmd3: start [bip] [level]")));
      send(msg.set(F("cmd4: password [pswd]")));
      send(msg.set(F("cmd5: buzzer [duration]")));
    } else {
      send(msg.set(F("command invalid")));
    }
  }
}

void loop() {
  //manageKeyboard();
  manageBuzzer();
  manageSiren();
  managePowerProbe();
  manageHeartbeat();
}

/*
  void correctPasswordEntered() {
  if (isSirenOn()) {
    stopSiren();
    send(msg.set(F("stopped by password")));
  } else {
    send(msg.set(F("password entered")));
  }
  }
*/

inline void managePowerProbe() {
  // if plugged to sector
  if (analogRead(POWER_PROBE) > 400) {
    if (_isOnBattery) {
      DEBUG_PRINT(F("On power supply"));
      send(msg.set(F("on power supply")));
      _isOnBattery = false;
      led.off();
      //startBuzzer(500);
    }
  } else {
    if (!_isOnBattery) {
      DEBUG_PRINT(F("On battery"));
      send(msg.set(F("on battery")));
      sendNrfStatus();
      _isOnBattery = true;
      led.setColor(RGBLed::RED);
      //startBuzzer(500);
    }
  }
}

void sendNrfStatus() {
  uint8_t status = RF24_getStatus();
  send(msg.set(status));
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

inline void manageSiren() {
  if (_sirenState != 0) {
    if ((_sirenState % 2 == 1 && _sirenState < _bipNumber * 2 + 2) && millis() - _sirenTime >= 1000) {
      startSirenSound(_sirenLevel);
      _sirenTime = millis();
      _sirenState++;
    } else if ((_sirenState % 2 == 0 && _sirenState < _bipNumber * 2 + 2) && millis() - _sirenTime >= 100) {
      stopSirenSound();
      _sirenTime = millis();
      _sirenState++;
    } else if ((_sirenState == _bipNumber * 2 + 2) && millis() - _sirenTime >= 180000) {
      DEBUG_PRINT(F("Siren stopped after 3 min!"));
      stopSiren();
      send(msg.set(F("siren stopped")));
    }
  }
}

/*
  inline void manageKeyboard() {
    if (_keyboardEnable) {
      char key = _keypad.getKey();

      // key pressed
      if (key != NO_KEY) {
        DEBUG_PRINT(F("Key pressed:"));
        DEBUG_PRINT(key);
        startBuzzer(50);

        if (key == _password[_keyboardPosition]) {
          _keyboardPosition++;
        } else {
          _keyboardPosition = 0;
        }

        if (_keyboardPosition == 4) {
          DEBUG_PRINT(F("Correct password entered!"));
          _keyboardPosition = 0;
          led.flash(RGBLed::GREEN, 100);
          startBuzzer(500);
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
*/

void startSirenSound(char level) {
  analogWrite(SIREN, map(level, 0, 100, 0, 255));
  led.setColor(RGBLed::GREEN);
}

void stopSirenSound() {
  analogWrite(SIREN, 0);

  if (_isOnBattery) {
    led.setColor(RGBLed::RED);
  } else {
    led.off();
  }
}

void startBuzzer(int duration) {
  _buzzerDuration = duration;
  _buzzerStartTime = millis();
  _buzzerIsOn = true;
  digitalWrite(BUZZER, HIGH);
}

void stopBuzzer() {
  digitalWrite(BUZZER, LOW);
  _buzzerIsOn = false;
}

inline void manageBuzzer() {
  if (_buzzerIsOn && millis() - _buzzerStartTime >= _buzzerDuration) {
    stopBuzzer();
  }
}

inline void manageHeartbeat() {
  static unsigned long _heartbeatLastSend = 0;
  static unsigned long _heartbeatWait = random(1000, 60000);
  static unsigned long _heartbeatRetryNb = 0;

  if (millis() - _heartbeatLastSend >= _heartbeatWait) {
    bool success = sendHeartbeat();

    if (success) {
      _heartbeatWait = 60000;
      _heartbeatRetryNb = 0;
    } else {
      if (_heartbeatRetryNb < 10) {
        _heartbeatWait = random(100, 3000);
        _heartbeatRetryNb++;
      } else {
        _heartbeatWait = random(45000, 60000);
        _heartbeatRetryNb = 0;
      }
    }
    
    _heartbeatLastSend = millis();
  }
}

/*
  void putKeypadToInterruptMode() {
  for(char i=0; i<_cols; i++) {
    pinMode(_colPins[i], OUTPUT);
    digitalWrite(_colPins[i], LOW);
  }

  pinMode(_rowPins[_rows-1], INPUT_PULLUP);
  }
*/

/*
  void initPasswordValue() {
  if (isAscii(loadState(0))) {
      for (char i=0; i<strlen(_password); i++) {
        _password[i] = loadState(i);
      }
  }

  DEBUG_PRINT(F("Password is:"));
  DEBUG_PRINT(_password);
  }

  void savePassword(const char* password) {
    for (char i=0; i<strlen(_password); i++) {
        _password[i] = password[i];
        saveState(i, _password[i]);
    }

  DEBUG_PRINT(F("New password saved:"));
  DEBUG_PRINT(_password);
  }

  void keyboardInterrupt() {
  _keyboardEnable = true;
  _keyboardEnableTime = millis();
  }
*/
