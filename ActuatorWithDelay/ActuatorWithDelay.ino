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
#define CHILD_ID_LEGACY 0
#define CHILD_ID_SWITCH 1
#define CHILD_ID_CONFIG_DELAY 2
#define EEPROM_DELAY 0

MyMessage msgLegacy(CHILD_ID_LEGACY, V_CUSTOM);
MyMessage msgSwitch(CHILD_ID_SWITCH, V_STATUS);
MyMessage msgConfigDelay(CHILD_ID_CONFIG_DELAY, V_TEXT);

// Relay
unsigned long _relayDuration = 0;
unsigned long _relayStartTime = 0;
bool _relayIsOn = false;
unsigned long _heartbeatTime = 0;
unsigned long _delayConfig = 0;

void before()
{
  // init PIN
  pinMode(RELAY1, OUTPUT);
  
  stopRelay();

  _delayConfig = getDelay();
}

void presentation() {
  sendSketchInfo("ActuatorWithDelay", "1.2");
  wait(100);

  // legacy child
  present(CHILD_ID_LEGACY, S_CUSTOM, "legacy message");
  wait(100);

  // relay on/off command
  present(CHILD_ID_SWITCH, S_BINARY, "On/off state");
  wait(100);

  // config time
  present(CHILD_ID_CONFIG_DELAY, S_INFO, "Time delay (s)");
  wait(100);
}

void setup() {
  sendBatteryLevel(0);
  send(msgLegacy.set(F("started")));
  send(msgSwitch.set(false)); // for Home Assistant to tell it's a switch
  send(msgConfigDelay.set((long) _delayConfig / 1000)); // for Home Assistant to tell it's a input text
}

void receive(const MyMessage &message)
{
  if (message.sensor == CHILD_ID_LEGACY && message.type == V_CUSTOM) {
    if (message.getLong() > 0) {
      startRelay(message.getLong() * 60000UL);
    } else {
      stopRelay();
    }
  } else if (message.sensor == CHILD_ID_SWITCH && message.type == V_STATUS) {
    if (message.getBool()) {
      startRelay(_delayConfig);
    } else {
      stopRelay();
    }
  } else if (message.sensor == CHILD_ID_CONFIG_DELAY && message.type == V_TEXT) {
    Serial.print("Time configuration received: ");
    const char *inputStr = message.getString();
    Serial.println(inputStr);

    char *endPtr;
    unsigned long configTime = strtoul(inputStr, &endPtr, 10);

    // If the end pointer points to the null character, the entire string was a valid number
    if (*endPtr == '\0') {
        Serial.print("Received valid configuration time: ");
        Serial.println(configTime);

        // Save the new delay value
        _delayConfig = configTime * 1000UL; 
        saveDelay(_delayConfig);

        // Send message to controller to confirm valid input
        send(msgConfigDelay.set((long) _delayConfig / 1000));
    } else {
        Serial.println("Received invalid configuration time");
        
        // Send message to controller to indicate invalid input
        send(msgConfigDelay.set("invalid"));
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
  send(msgLegacy.set(F("powered on")));
  delay(100);
  send(msgSwitch.set(true));
}

void stopRelay() {
  digitalWrite(RELAY1, false);
  _relayIsOn = false;
  send(msgLegacy.set(F("powered off")));
  delay(100);
  send(msgSwitch.set(false));
}

inline void manageRelay() {
  if (_relayIsOn && _relayDuration > 0 && millis() - _relayStartTime >= _relayDuration) {
    stopRelay();
  }      
}

inline void manageHeartbeat() {
  if (millis() - _heartbeatTime >= 3600000) {
    sendHeartbeat();
    _heartbeatTime = millis();
  }
}

void saveDelay(unsigned long value) {
  byte *b = (byte *)&value;

  for (byte i = 0; i < sizeof(value); i++) {
    saveState(EEPROM_DELAY + i, b[i]);
  }
}

unsigned long getDelay() {
  unsigned long value = 0;
  byte *b = (byte *)&value;

  for (byte i = 0; i < sizeof(value); i++) {
    *b++ = loadState(EEPROM_DELAY + i);
  }

  return value;
}
