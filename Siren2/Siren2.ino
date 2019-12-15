#define MY_DEBUG
//#define DHT_SENSOR
#define PIR
#define RF

#define MY_RADIO_RF24
#define MY_RX_MESSAGE_BUFFER_FEATURE
#define MY_RX_MESSAGE_BUFFER_SIZE (10)
#define MY_RF24_IRQ_PIN (2)
#define MY_RF24_PA_LEVEL (RF24_PA_HIGH)
#define MY_REPEATER_FEATURE

// Define pins
#define SIREN_PIN 7
#define GREEN_LED_PIN A1
#define RED_LED_PIN A0
#define POWER_PROBE_PIN A6
#define DHT_PIN 4
#define PIR_PIN A2
#define BUTTON1_PIN 5
#define BUTTON2_PIN 6
#define RF_PIN 3

// Debug print
#if defined(MY_DEBUG)
#define DEBUG_PRINT(str) Serial.println(str);
#else
#define DEBUG_PRINT(str)
#endif

// Includes
#include <MySensors.h>
#include <Parser.h>
#include <Bounce2.h>
#include <Timer.h>

// Child ID
#define CHILD_ID_SIREN 0
#define CHILD_ID_HUM 1
#define CHILD_ID_TEMP 2
#define CHILD_ID_PIR 3
#define CHILD_ID_RF 4

// Siren
MyMessage msgSiren(CHILD_ID_SIREN, V_CUSTOM);
unsigned long _sirenTime = 0;
int _sirenState = 0;
byte _bipNumber = 0;
byte _sirenLevel = 100;

// DHT
#if defined(DHT_SENSOR)
#include <DHT.h>
MyMessage msgHum(CHILD_ID_HUM, V_HUM);
MyMessage msgTemp(CHILD_ID_TEMP, V_TEMP);
DHT dht;
unsigned long lastSendTemperatureTime = 0;
#endif

// PIR
#if defined(PIR)
MyMessage msgPir(CHILD_ID_PIR, V_TRIPPED);
bool sendPirValue = false;
#endif

// RF 433MhZ
#if defined(RF)
#include <RCSwitch.h>
RCSwitch rcSwitch = RCSwitch();
unsigned long oldSenderRf = 0;
unsigned long oldSenderTfTime = 0;
MyMessage msgRf(CHILD_ID_RF, V_CUSTOM);
#endif

// Buttons
const byte buttons[] = {BUTTON1_PIN, BUTTON2_PIN};
Bounce* keys[sizeof(buttons)];
int button1OldValue = -1;
byte _password[4] = {1, 2, 2, 1};
int _passwordPosition = 0;
unsigned long _keyboardEnableTime = 0;

// Leds
bool greenLedOn = false;
bool redLedOn = false;

// Others
bool _isOnBattery = false;
Parser parser = Parser(' ');
Timer timer;

void before()
{
  pinMode(SIREN_PIN, OUTPUT);
  pinMode(POWER_PROBE_PIN, INPUT);
  pinMode(PIR_PIN, INPUT);
  pinMode(GREEN_LED_PIN, OUTPUT);
  pinMode(RED_LED_PIN, OUTPUT);

  stopSiren();
  setGreenLedOn(true);
  setRedLedOn(true);
}

void setup() {
  setRedLedOn(false);
#if defined(DHT_SENSOR)
  dht.setup(DHT_PIN);
  sleep(dht.getMinimumSamplingPeriod());
#endif
  
#if defined(RF)
  rcSwitch.enableReceive(digitalPinToInterrupt(RF_PIN));
#endif

  for (byte i = 0; i < sizeof(buttons); i++) {
    keys[i] = new Bounce();
    keys[i]->attach(buttons[i], INPUT_PULLUP);
    keys[i]->interval(25);
  }

  for (byte i=0; i<10; i++) {
    if (i%2 == 0) {
      setGreenLedOn(false);
      setRedLedOn(true);
    } else {
      setGreenLedOn(true);
      setRedLedOn(false);
    }
    delay(100);
  }


  send(msgSiren.set(F("system started")));

  setGreenLedOn(false);
  setRedLedOn(false);
}

void presentation() {
  sendSketchInfo("Siren 2", "1.0");
  present(CHILD_ID_SIREN, S_CUSTOM);
  present(CHILD_ID_HUM, S_HUM);
  present(CHILD_ID_TEMP, S_TEMP);
  present(CHILD_ID_PIR, S_MOTION);
  present(CHILD_ID_RF, S_CUSTOM);
}

void receive(const MyMessage &myMsg)
{
  if (myMsg.type == V_CUSTOM && myMsg.sensor == 0) {
    parser.parse(myMsg.getString());

    if (parser.get(0) == NULL) {
      send(msgSiren.set(F("cmd missing! send help")));
    } else if (parser.isEqual(0, "ping")) {
      // nothing to do
    } else if (parser.isEqual(0, "stop")) {
      stopSiren();
      send(msgSiren.set(F("siren stopped by user")));
    } else if (parser.isEqual(0, "start") && parser.get(1) != NULL && parser.get(2) != NULL) {
      startSiren(parser.getInt(1), parser.getInt(2));
      send(msgSiren.set(F("siren started")));
    } else {
      send(msgSiren.set(F("command invalid")));
    }
  }
}

void loop() {
  manageSiren();
  //managePowerProbe();
  manageDhtSensor();
  managePirSensor();
  manageRfReceptor();
  manageButtons();
  timer.update();
}

inline void manageRfReceptor() {
#if defined(RF)
  if (rcSwitch.available()) {
    unsigned long value = rcSwitch.getReceivedValue();
    if (value != 0 && (value != oldSenderRf || millis() - oldSenderTfTime > 1000)) {
      send(msgRf.set(value));
      oldSenderRf = value;
      oldSenderTfTime = millis();
    }
    rcSwitch.resetAvailable();
  }
#endif
}

inline void manageButtons() {
  // get pressed button number
  byte button = 0;
  for (byte i = 0; i < sizeof(buttons); i++) {
    keys[i]->update();
    if (keys[i]->fell()) {
      button += i + 1;
      _keyboardEnableTime = millis();
    }
  }

  // Button pressed
  if (button != -0) {
    DEBUG_PRINT(F("Button pressed:"));
    DEBUG_PRINT(button);

    if (button == _password[_passwordPosition]) {
      _passwordPosition++;
    } else {
      _passwordPosition = 0;
    }

    if (_passwordPosition == 4) {
      DEBUG_PRINT(F("Correct password entered!"));
      _passwordPosition = 0;
      timer.pulseImmediate(GREEN_LED_PIN, 3000, HIGH);

      if (isSirenOn()) {
        stopSiren();
        send(msgSiren.set(F("stopped by password")));
      } else {
        send(msgSiren.set(F("password entered")));
      }
    }
  }

  if (_passwordPosition !=0 && millis() - _keyboardEnableTime >= 2000) {
    _passwordPosition = 0;
  }
}

inline void manageDhtSensor() {
#if defined(DHT_SENSOR)
  if ((millis() - lastSendTemperatureTime) >= 600000) {
    lastSendTemperatureTime = millis();
    dht.readSensor(true); // Force reading sensor, so it works also after sleep
    
    float temperature = dht.getTemperature();
    if (!isnan(temperature)) {
      send(msgTemp.set(temperature, 1));
    }
    
    float humidity = dht.getHumidity();
    if (!isnan(humidity)) {
      send(msgHum.set(humidity, 1));
    }
  }
#endif
}

inline void managePirSensor() {
#if defined(PIR)
  bool tripped = digitalRead(PIR_PIN) == HIGH;

  if (tripped != sendPirValue) {
    DEBUG_PRINT("PIR: event");
    send(msgPir.set(tripped ? "1" : "0"));
    sendPirValue = tripped;

    if (tripped) {
      setGreenLedOn(true);
    } else {
      setGreenLedOn(false);
    }
  }
#endif
}

inline void managePowerProbe() {
  // if plugged to sector
  if (analogRead(POWER_PROBE_PIN) > 400) {
    if (_isOnBattery) {
      DEBUG_PRINT(F("On power supply"));
      send(msgSiren.set(F("on power supply")));
      _isOnBattery = false;
    }
  } else {
    if (!_isOnBattery) {
      DEBUG_PRINT(F("On battery"));
      send(msgSiren.set(F("on battery")));
      _isOnBattery = true;
    }
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
    } else if ((_sirenState == _bipNumber * 2 + 2) && millis() - _sirenTime >= 60000) {
      DEBUG_PRINT(F("Siren stopped after 60s!"));
      stopSiren();
      send(msgSiren.set(F("siren stopped")));
    }
  }
}

void startSirenSound(char level) {
  analogWrite(SIREN_PIN, map(level, 0, 100, 0, 255));
}

void stopSirenSound() {
  analogWrite(SIREN_PIN, 0);
}

void setGreenLedOn(bool on) {
  if (on) {
    digitalWrite(GREEN_LED_PIN, HIGH);
  } else {
    digitalWrite(GREEN_LED_PIN, LOW);
  }
}

void setRedLedOn(bool on) {
  if (on) {
    digitalWrite(RED_LED_PIN, HIGH);
  } else {
    digitalWrite(RED_LED_PIN, LOW);
  }
}
