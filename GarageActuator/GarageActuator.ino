// Enable debug prints to serial monitor
#define MY_DEBUG

#define MY_BAUD_RATE (9600ul)

// Enable and select radio type attached
#define MY_RADIO_RF24

// Enable IRQ pin
//#define MY_RX_MESSAGE_BUFFER_FEATURE
//#define MY_RF24_IRQ_PIN (2)

// RF24 PA level
#define MY_RF24_PA_LEVEL (RF24_PA_HIGH)

#include <MySensors.h>
#include <Servo.h>
#include <SoftwareSerial.h>

#define SERVO1_PIN 3
#define SERVO2_PIN 6
#define BLE_RX_PIN 5
#define BLE_TX_PIN 8
#define BLE_LINK_PIN 7
#define LED_PIN A0

#define SERVO1_UNLOCK_POS 0
#define SERVO1_LOCK_POS 180
#define SERVO2_UNLOCK_POS 180
#define SERVO2_LOCK_POS 0

#define PASSWORD_LENGTH 10 // + 1 for then end of string (0)
#define BUFFER_SIZE 100

#define EEPROM_PASSWORD_POS 10
#define EEPROM_SERVO1_POS 0
#define EEPROM_SERVO2_POS 1

MyMessage msg(1, V_CUSTOM);
SoftwareSerial ble(BLE_TX_PIN, BLE_RX_PIN); // RX, TX

Servo _servo1;
Servo _servo2;
unsigned long _timeOfLastChange = 0;
bool _action = false;
byte _servo1_position;
byte _servo1_target;
byte _servo2_position;
byte _servo2_target;
bool _ledOn = false;
char _password[PASSWORD_LENGTH];
char _serialBuffer[BUFFER_SIZE];
char _bleBuffer[BUFFER_SIZE];

void before()
{
  _servo1_position = loadState(EEPROM_SERVO1_POS); // Set position to last known state (using eeprom storage)
  _servo2_position = loadState(EEPROM_SERVO2_POS); // Set position to last known state (using eeprom storage)
  _action = false;

  initPasswordValue();

  pinMode(BLE_LINK_PIN, INPUT);
  pinMode(LED_PIN, OUTPUT);

  if (isLocked()) {
    powerOnLed();
  } else {
    powerOffLed();
  }
}

void setup() {
  randomSeed(analogRead(1)); // A1
  ble.begin(9600);
}

void presentation() {
  // Send the sketch version information to the gateway and Controller
  sendSketchInfo("GarageActuator", "1.0");

  // Present sensor to controller
  present(0, S_BINARY);
  present(1, S_CUSTOM);
}

void receive(const MyMessage &message)
{
  if (message.type == V_STATUS && message.sensor == 0) {
    if (message.getBool()) {
      Serial.println("lock cmd received");
      lock();
    } else {
      Serial.println("unlock cmd received");
      unlock();
    }
  }
}

inline void manageServo() {
  if (_action && millis() - _timeOfLastChange > 5) {
    // start servo 1
    if (_servo1_target != _servo1_position && !_servo1.attached() && !_servo2.attached()) {
      send(msg.set(F("moving servo 1...")));
      _servo1.attach(SERVO1_PIN);
    }

    // start servo 2
    if (_servo2_target != _servo2_position && !_servo1.attached() && !_servo2.attached()) {
      send(msg.set(F("moving servo 2...")));
      _servo2.attach(SERVO2_PIN);
    }

    // Move servo 1
    if (_servo1.attached()) {
      if (_servo1_target > _servo1_position) {
        _servo1_position++;
        _servo1.write(_servo1_position);
      } else if (_servo1_target < _servo1_position) {
        _servo1_position--;
        _servo1.write(_servo1_position);
      } else {
        if (_servo1_target == SERVO1_UNLOCK_POS) {
          send(msg.set(F("servo 1 unlocked")));
        } else {
          send(msg.set(F("servo 1 locked")));
        }

        // save in eeprom
        if (loadState(EEPROM_SERVO1_POS) != _servo1_target) {
          saveState(EEPROM_SERVO1_POS, _servo1_target);
        }

        _servo1.detach();
      }
      // Move servo 2
    } else if (_servo2.attached()) {
      if (_servo2_target > _servo2_position) {
        _servo2_position++;
        _servo2.write(_servo2_position);
      } else if (_servo2_target < _servo2_position) {
        _servo2_position--;
        _servo2.write(_servo2_position);
      } else {
        if (_servo2_target == SERVO2_UNLOCK_POS) {
          send(msg.set(F("servo 2 unlocked")));
        } else {
          send(msg.set(F("servo 2 locked")));
        }

        // save in eeprom
        if (loadState(EEPROM_SERVO2_POS) != _servo2_target) {
          saveState(EEPROM_SERVO2_POS, _servo2_target);
        }

        _servo2.detach();
      }
    } else if (_servo1_target == _servo1_position && _servo2_target == _servo2_position) {
      _action = false;

      if (_servo1_target == SERVO1_UNLOCK_POS) {
        powerOffLed();
        send(msg.set(F("Unlocked")));
      } else {
        powerOnLed();
        send(msg.set(F("Locked")));
      }
    }

    _timeOfLastChange = millis();
  }
}

void loop() {
  if (bleDataAvailable()) {
    Serial.print("ble: ");
    Serial.println(_bleBuffer);

    if (startWith(_bleBuffer, "cmd+unlock=")) {
      if (equal(_bleBuffer + 11, _password)) {
        unlock();
        ble.println("status:unlock");
        Serial.println("unlock");
      } else {
        ble.println("status:wrong password");
      }
    } else if (startWith(_bleBuffer, "cmd+lock=")) {
      if (equal(_bleBuffer + 9, _password)) {
        lock();
        ble.println("status:lock");
        Serial.println("lock");
      } else {
        ble.println("status:wrong password");
      }
    } else if (startWith(_bleBuffer, "cmd+data?")) {
      if (isLocked()) {
        ble.println("status:lock");
      } else {
        ble.println("status:unlock");
      }
    }
  }

  if (serialDataAvailable()) {
    if (startWith(_serialBuffer, "password:")) {
      savePassword(_serialBuffer + 9);
    } else {
      Serial.println(_serialBuffer);
      ble.write(_serialBuffer);
    }
  }

  manageServo();
  manageHeartbeat();
  manageLed();
  manageBleLink();
}

inline void manageBleLink() {
  static bool linked = false;
  static unsigned long timer = 0;
  
  if (digitalRead(BLE_LINK_PIN) == HIGH && linked == false) {
    linked = true;
    timer = millis();
  }

  if (digitalRead(BLE_LINK_PIN) == LOW && linked == true) {
    linked = false;
  }

  if (linked && millis() - timer >= 60000UL) {
    disconnectBle();
  }
}

bool isBleLinked() {
  return digitalRead(BLE_LINK_PIN) == HIGH;
}

void disconnectBle() {
  ble.write("+++");
  delay(300);
  ble.write("at+discon=0");
  delay(300);
  ble.write("at+discon=1");
  delay(300);
  ble.write("at+exit");
  Serial.println("Ble Disconected by timeout");
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

inline void manageLed() {
  static unsigned long timer = 0;

  if (_action) {
    if (millis() - timer >= 250) {
      timer = millis();

      if (_ledOn) {
        powerOffLed();
      } else {
        powerOnLed();
      }
    }
  }
}

void powerOnLed() {
  digitalWrite(LED_PIN, HIGH);
  _ledOn = true;
}

void powerOffLed() {
  digitalWrite(LED_PIN, LOW);
  _ledOn = false;
}

void unlock() {
  _servo1_target = SERVO1_UNLOCK_POS;
  _servo2_target = SERVO2_UNLOCK_POS;
  _action = true;
  powerOnLed();
  _timeOfLastChange = millis();
}

void lock() {
  _servo1_target = SERVO1_LOCK_POS;
  _servo2_target = SERVO2_LOCK_POS;
  _action = true;
  powerOnLed();
  _timeOfLastChange = millis();
}

bool isLocked() {
  return _servo1_position == SERVO1_LOCK_POS && _servo2_target == SERVO2_LOCK_POS;
}

void initPasswordValue() {
  for (unsigned char i = 0; i < PASSWORD_LENGTH; i++) {
    _password[i] = loadState(EEPROM_PASSWORD_POS + i);
  }

  Serial.print(F("Password is:"));
  Serial.println(_password);
}

void savePassword(const char* password) {
  if (strlen(password) > PASSWORD_LENGTH - 1) {
    Serial.println("Password too long");
  } else {
    for (unsigned char i = 0; i < PASSWORD_LENGTH; i++) {
      if (i < strlen(password)) {
        _password[i] = password[i];
      } else {
        _password[i] = 0; // end string
      }

      saveState(EEPROM_PASSWORD_POS + i, _password[i]);
    }

    Serial.print(F("New password saved:"));
    Serial.println(_password);
  }
}

inline bool serialDataAvailable() {
  if (Serial.available()) {
    unsigned char cpt = 0;

    while (Serial.available()) {
      _serialBuffer[cpt] = Serial.read();

      if (cpt > BUFFER_SIZE - 1 || _serialBuffer[cpt] == '\n') {
        _serialBuffer[cpt] = 0; // end string
        break;
      } else {
        unsigned long timer = millis();
        while (!Serial.available() && (millis() - timer < 1000));
        cpt++;
        _serialBuffer[cpt] = 0; // end string
      }
    }

    return true;
  } else {
    return false;
  }
}


inline bool bleDataAvailable() {
  if (ble.available()) {
    unsigned char cpt = 0;

    while (ble.available()) {
      _bleBuffer[cpt] = ble.read();

      if (cpt > BUFFER_SIZE - 1 || _bleBuffer[cpt] == '\n') {
        _bleBuffer[cpt] = 0; // end string
        break;
      } else {
        unsigned long timer = millis();
        while (!ble.available() && (millis() - timer < 1000));
        cpt++;
        _bleBuffer[cpt] = 0; // end string
      }
    }

    return true;
  } else {
    return false;
  }
}

bool startWith(char *str, char *sfind)
{
  if (strlen(sfind) > strlen(str)) {
    return false;
  }

  for (char i = 0; i < strlen(sfind); i++) {
    if (str[i] != sfind[i]) {
      return false;
    }
  }

  return true;
}

bool equal(char *str, char *sfind)
{
  if (strlen(sfind) != strlen(str)) {
    return false;
  }

  for (char i = 0; i < strlen(sfind); i++) {
    if (str[i] != sfind[i]) {
      return false;
    }
  }

  return true;
}
