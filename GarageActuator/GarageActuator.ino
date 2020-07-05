// Enable debug prints to serial monitor
#define MY_DEBUG

// Enable and select radio type attached
#define MY_RADIO_RF24

// Enable IRQ pin
#define MY_RX_MESSAGE_BUFFER_FEATURE
#define MY_RF24_IRQ_PIN (2)

// RF24 PA level
#define MY_RF24_PA_LEVEL (RF24_PA_HIGH)

#include <MySensors.h>
#include <Servo.h>
#include <SoftwareSerial.h>

#define SERVO1_PIN 3
#define SERVO2_PIN 5
#define SERVO1_UNLOCK_POS 0
#define SERVO1_LOCK_POS 180
#define SERVO2_UNLOCK_POS 180
#define SERVO2_LOCK_POS 0

MyMessage msg(1, V_CUSTOM);

Servo _servo1;
Servo _servo2;
unsigned long _timeOfLastChange = 0;
bool _action = false;
byte _servo1_position;
byte _servo1_target;
byte _servo2_position;
byte _servo2_target;

SoftwareSerial hc06(7, 8); // RX, TX

void before()
{
  _servo1_position = loadState(0); // Set position to last known state (using eeprom storage)
  _servo2_position = loadState(1); // Set position to last known state (using eeprom storage)
  _action = false;
}

void setup() {
  randomSeed(analogRead(0)); // A0
  hc06.begin(9600);
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
      _servo1_target = SERVO1_LOCK_POS;
      _servo2_target = SERVO2_LOCK_POS;
    } else {
      Serial.println("unlock cmd received");
      _servo1_target = SERVO1_UNLOCK_POS;
      _servo2_target = SERVO2_UNLOCK_POS;
    }

    _action = true;
    _timeOfLastChange = millis();
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
        if (loadState(0) != _servo1_target) {
          saveState(0, _servo1_target);
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
        if (loadState(1) != _servo2_target) {
          saveState(1, _servo2_target);
        }

        _servo2.detach();
      }
    } else if (_servo1_target == _servo1_position && _servo2_target == _servo2_position) {
      _action = false;

      if (_servo1_target == SERVO1_UNLOCK_POS) {
        send(msg.set(F("Unlocked")));
      } else {
        send(msg.set(F("Locked")));
      }
    }

    _timeOfLastChange = millis();
  }
}

void loop() {
  if (hc06.available() > 0) {
    char incomingChar = hc06.read();
    Serial.println("HC-06 message received");

    if (incomingChar == '0') {
      _servo1_target = SERVO1_UNLOCK_POS;
      _servo2_target = SERVO2_UNLOCK_POS;
      _action = true;
      hc06.write("Unlock");
    } else if (incomingChar == '1') {
      _servo1_target = SERVO1_LOCK_POS;
      _servo2_target = SERVO2_LOCK_POS;
      _action = true;
      hc06.write("Lock");
    }
  }
  
  manageServo();
  manageHeartbeat();
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
