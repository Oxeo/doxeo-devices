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

#define RELAY1 5

MyMessage msg(0, V_CUSTOM);

// Relay
unsigned long _relayDuration = 0;
unsigned long _relayStartTime = 0;
bool _relayIsOn = false;
unsigned long _heartbeatTime = 0;

void before()
{
  // init PIN
  pinMode(RELAY1, OUTPUT);
  
  stopRelay();
}

void setup() {
  send(msg.set(F("started")));
}

void presentation() {
  sendSketchInfo("ActuatorWithDelay", "1.1");

  // Present sensor to controller
  present(0, S_CUSTOM);
}

void receive(const MyMessage &message)
{
  if (message.type == V_CUSTOM && message.sensor == 0) {
    if (message.getLong() > 0) {
      startRelay(message.getLong() * 60000UL);
    } else {
      stopRelay();
    }
  }
}

void loop() {
  manageRelay();
  manageHeartbeat();
}

void startRelay(unsigned long duration) {
  _relayDuration = duration;
  _relayStartTime = millis();
  _relayIsOn = true;
  digitalWrite(RELAY1, true);
  send(msg.set(F("powered on")));
}

void stopRelay() {
  digitalWrite(RELAY1, false);
  _relayIsOn = false;
  send(msg.set(F("powered off")));
}

inline void manageRelay() {
  if (_relayIsOn && millis() - _relayStartTime >= _relayDuration) {
    stopRelay();
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
