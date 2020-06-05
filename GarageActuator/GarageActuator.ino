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

#define SERVO1_PIN 3
#define SERVO2_PIN 5
#define SERVO1_MIN 0
#define SERVO1_MAX 180
#define SERVO2_MIN 0
#define SERVO2_MAX 180

MyMessage msg(0, V_STATUS);

Servo _servo1;
Servo _servo2;
unsigned long _timeOfLastChange = 0;
bool _servo1_action = false;
byte _servo1_position;
byte _servo1_target;

unsigned long _heartbeatTime = 0;

void before()
{
  _servo1_position = loadState(0); // Set position to last known state (using eeprom storage)
  _servo1.attach(SERVO1_PIN);
  _servo1.write(_servo1_position);
  delay(100);
  _servo1.detach();
  _servo1_action = false;
}

void setup() {
 
}

void presentation() {
  // Send the sketch version information to the gateway and Controller
  sendSketchInfo("GarageActuator", "1.0");

  // Present sensor to controller
  present(0, S_BINARY);
}

void receive(const MyMessage &message)
{
  if (message.type == V_STATUS && message.sensor == 0) {
    _servo1_action = true;
    _timeOfLastChange = millis();
  
    if (message.getBool()) {
      Serial.println("lock");
      _servo1_target = SERVO1_MAX;
    } else {
      Serial.println("unlock");
      _servo1_target = SERVO1_MIN;
    }

    // save in eeprom
    if (loadState(0) != _servo1_target) {
      Serial.println("save in epprom");
      saveState(0, _servo1_target);
    }
  }
}

inline void manageServo() {
  if (_servo1_action && millis() - _timeOfLastChange > 5) {
    if(!_servo1.attached()) {
      Serial.println("attach servo 1");
      _servo1.attach(SERVO1_PIN);
    }
    
    if (_servo1_target > _servo1_position) {
      _servo1_position++;
      _servo1.write(_servo1_position);
    } else if (_servo1_target < _servo1_position) {
      _servo1_position--;
      _servo1.write(_servo1_position);
    } else {
      if (_servo1.read() == _servo1_target) {
        Serial.println("servo 1 position is ok");
      } else {
        Serial.println("servo 1 position is KO");
      }
      
      _servo1.detach();
      _servo1_action = false;
    }

    _timeOfLastChange = millis();
  }
}

void loop() {
  manageServo();
  
  if (millis() - _heartbeatTime >= 60000) {
    sendHeartbeat();
    _heartbeatTime = millis();
  }
}
