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

#define RELAY1 8
#define BUZZER 7

MyMessage msg(0, V_STATUS);

// Buzzer
int _buzzerDuration = 0;
unsigned long _buzzerStartTime = 0;
bool _buzzerIsOn = false;

unsigned long _heartbeatTime = 0;

void before()
{
  // init PIN
  pinMode(RELAY1, OUTPUT);
  pinMode(BUZZER, OUTPUT);

  // Set relay to last known state (using eeprom storage)
  digitalWrite(RELAY1, loadState(0));

  stopBuzzer();
}

void setup() {
  startBuzzer(50);
}

void presentation() {
  // Send the sketch version information to the gateway and Controller
  sendSketchInfo("Actuator", "1.0");

  // Present sensor to controller
  present(0, S_BINARY);
}

void receive(const MyMessage &message)
{
  if (message.type == V_STATUS && message.sensor == 0) {
    // Change relay state
    digitalWrite(RELAY1, message.getBool());

    // Store state in eeprom
    if (loadState(message.sensor) != message.getBool()) {
      saveState(message.sensor, message.getBool());
    }

    if (message.getBool()) {
      startBuzzer(1000);
    } else {
      startBuzzer(100);
    }
  }
}

void loop() {
  manageBuzzer();

  if (millis() - _heartbeatTime >= 60000) {
    sendHeartbeat();
    _heartbeatTime = millis();
  }
}

void startBuzzer(int duration) {
  _buzzerDuration = duration;
  _buzzerStartTime = millis();
  _buzzerIsOn = true;
  tone(BUZZER, 1000); // Send 1KHz sound signal...
}

void stopBuzzer() {
  noTone(BUZZER);
  _buzzerIsOn = false;
}

inline void manageBuzzer() {
  if (_buzzerIsOn && millis() - _buzzerStartTime >= _buzzerDuration) {
    stopBuzzer();
  }
}
