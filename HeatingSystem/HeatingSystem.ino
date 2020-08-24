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
#include <RGBLed.h>

#define RELAY 3
#define RED_LED A0
#define GREEN_LED A1
#define BLUE_LED A2

enum Status {
  OFF,
  COOL,
  HEAT
};

MyMessage _myMsg(0, V_CUSTOM);
RGBLed _led(RED_LED, GREEN_LED, BLUE_LED, COMMON_CATHODE);
Status _status = OFF;
unsigned long lastSendStatus = 0;

void before()
{
  // init PIN
  pinMode(RELAY, OUTPUT);

  // init status from eprom
  if (loadState(0) < 3) {
    setStatus(loadState(0));
  } else {
    setStatus(OFF);
  }
}

void setup() {
  send(_myMsg.set(F("started")));
}

void presentation() {
  sendSketchInfo("HeatingSystem", "1.0");

  // Present sensor to controller
  present(0, S_CUSTOM);
}

void receive(const MyMessage &message)
{
  if (message.type == V_CUSTOM && message.sensor == 0) {
    String str = message.getString();

    if (str == "heat") {
      setStatus(HEAT);
      send(_myMsg.set(F("set to heat")));
    } else if (str == "cool") {
      setStatus(COOL);
      send(_myMsg.set(F("set to cool")));
    } else if (str == "off") {
      setStatus(OFF);
      send(_myMsg.set(F("set to off")));
    }
  }
}

void loop() {
  if (_status == HEAT && millis() - lastSendStatus >= 3600000) {
    send(_myMsg.set(F("heat"))); 
    lastSendStatus = millis();
  }
  
  manageHeartbeat();
}

void setStatus(Status status) {
  if (_status != status) {
    saveState(0, status);
    _status = status;
  }

  switch (status) {
    case OFF:
      Serial.println("OFF status");
      _led.off();
      digitalWrite(RELAY, false);
      break;
    case COOL:
      Serial.println("COOL status");
      _led.setColor(RGBLed::BLUE);
      digitalWrite(RELAY, false);
      break;
    case HEAT:
      Serial.println("HEAT status");
      _led.setColor(RGBLed::RED);
      digitalWrite(RELAY, true);
      lastSendStatus = millis();
      break;
  }
}

inline void manageHeartbeat() {
  static unsigned long lastSend = 0;
  static unsigned long waitTime = random(1000, 60000);
  static unsigned long retryNb = 0;

  if (millis() - lastSend >= waitTime) {
    bool success = sendHeartbeat();

    if (success) {
      waitTime = 60000;
      retryNb = 0;
    } else {
      if (retryNb < 10) {
        waitTime = random(100, 3000);
        retryNb++;
      } else {
        waitTime = random(45000, 60000);
        retryNb = 0;
      }
    }
    
    lastSend = millis();
  }
}
