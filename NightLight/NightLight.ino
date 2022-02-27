#include "Arduino.h"
#include <EEPROM.h>
#include <SoftwareSerial.h>
#include <FastLED.h>

#define BLE_RX_PIN 5
#define BLE_TX_PIN 6
#define BLE_DATA_PIN 2
#define BLE_LINK_PIN 7
#define BLE_WKP_PIN 4
#define BUTTON_PIN 3
#define LED_PIN 9

#define NUM_LEDS 6
#define NUM_ANIMATION 1

#define EEPROM_COLOR 0      // 3 byte
#define EEPROM_TIMER 4      // 1 byte
#define EEPROM_ANIMATION 5  // 1 byte

SoftwareSerial ble(BLE_TX_PIN, BLE_RX_PIN); // RX, TX
CRGB leds[NUM_LEDS];

unsigned long _startTime = 0;
unsigned long _timer;
boolean _lightIsOn = false;
byte _color[3];

int _animationSelected = 0;
unsigned long _animationTimer = 0;
int _animationCpt = 0;
int _animationStep = 0;

void setup() {
  randomSeed(analogRead(0));
  Serial.begin(9600);
  ble.begin(9600);

  pinMode(BLE_DATA_PIN, INPUT);
  pinMode(BLE_LINK_PIN, INPUT);
  pinMode(BLE_WKP_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  FastLED.addLeds<WS2812, LED_PIN, GRB>(leds, NUM_LEDS);

  Serial.println("Timer: " + String(getTimer()) + " hour(s)");
  Serial.println("Animation: " + String(getAnimation()));

  _timer = getTimer() * 3600000UL;
  _animationSelected = getAnimation();
  readColor();

  for (int i = 0; i < NUM_LEDS; i++) {
    leds[i] = CRGB ( 0, 0, 255);
    FastLED.show();
    delay(100);
  }

  for (int i = (NUM_LEDS - 1) ; i >= 0; i--) {
    leds[i] = CRGB (0, 0, 0);
    FastLED.show();
    delay(100);
  }

  if (digitalRead(BLE_LINK_PIN) == HIGH) {
    sendDataToBleDevice();
  }
}

void loop() {
  while (ble.available()) {
    String msg = ble.readStringUntil('\n');
    Serial.println("ble: " + msg);

    if (msg == "cmd+start") {
      ble.println("status:on");
      startLight();
    } else if (msg == "cmd+stop") {
      ble.println("status:off");
      stopLight();
    } else if (msg == "cmd+data?") {
      sendDataToBleDevice();
    } else if (msg.startsWith("cmd+color=")) {
      String color = parseCommand(msg, '=', 1);
      displayColor(color);
    } else if (msg.startsWith("cmd+savecolor=")) {
      String color = parseCommand(msg, '=', 1);
      displayColor(color);
      saveColor();
      Serial.println("Color saved: " + color);
      ble.println("color:" + color);

      if (!_lightIsOn) {
        stopLight();
      }
    } else if (msg.startsWith("cmd+timer=")) {
      byte timer = parseCommand(msg, '=', 1).toInt();
      saveTimer(timer);
      Serial.println("Timer: " + String(timer));
      ble.println("timer:" + String(timer));
      _timer = getTimer() * 60000;
    } else if (msg.startsWith("cmd+animation=")) {
      byte animation = parseCommand(msg, '=', 1).toInt();
      saveAnimation(animation);
      _animationSelected = animation;
      Serial.println("Animation: " + String(animation));
      ble.println("animation:" + String(animation));
    } else if (msg.startsWith("cmd+name=")) {
      String name = parseCommand(msg, '=', 1);
      Serial.println("Name: " + String(name));
      ble.print("+++");
      delay(500);
      ble.print("AT+NAME=[onl] " + name);
      delay(500);
      ble.print("AT+EXIT");
    }
  }

  if (Serial.available() > 0) {
    char incomingByte = Serial.read();
    ble.write(incomingByte);
  }

  if (_lightIsOn && (millis() - _startTime >= _timer)) {
    stopLight();
  }

  if (_lightIsOn) {
    switch (_animationSelected) {
      case 0:
        break;
      case 1:
        animation1();
        break;
      case 2:
        animation2();
        break;
    }
  }

  if (digitalRead(BUTTON_PIN) == LOW) {
    if (_lightIsOn) {
      stopLight();
    } else {
      startLight();
    }

    if (digitalRead(BLE_LINK_PIN) == HIGH) {
      sendDataToBleDevice();
    }

    delay(500);
  }
}

void sendDataToBleDevice() {
  String status =  _lightIsOn ? "on" : "off";
  ble.println("status:" + status);

  String hexstring = "";
  for (int i = 0; i < 3; i++) {
    if (_color[i] < 16) {
      hexstring += "0" + String(_color[i], HEX);
    } else {
      hexstring += String(_color[i], HEX);
    }
  }

  Serial.println("Color: " + hexstring);
  ble.println("color:" + hexstring);

  ble.println("timer:" + String(getTimer()));
  ble.println("animation:" + String(getAnimation()));
}

void startLight() {
  for (int i = 0; i < NUM_LEDS; i++) {
    leds[i] = CRGB (_color[0], _color[1], _color[2]);
    FastLED.show();
    delay(20);
  }

  initAnimation();
  _startTime = millis();
  _lightIsOn = true;
}

void stopLight() {
  for (int i = (NUM_LEDS - 1) ; i >= 0; i--) {
    leds[i] = CRGB (0, 0, 0);
    FastLED.show();
    delay(10);
  }

  _lightIsOn = false;
}

void displayColor(String color) {
  if (color.length() != 6) {
    Serial.println("Color not valid!");
  }

  char firstByte[3];
  color.substring(0, 2).toCharArray(firstByte, 3);
  _color[0] = (byte) strtol(firstByte, NULL, 16);

  color.substring(2, 4).toCharArray(firstByte, 3);
  _color[1] = (byte) strtol(firstByte, NULL, 16);

  color.substring(4, 6).toCharArray(firstByte, 3);
  _color[2] = (byte) strtol(firstByte, NULL, 16);

  for (int i = 0; i < NUM_LEDS; i++) {
    leds[i] = CRGB (_color[0], _color[1], _color[2]);
  }

  FastLED.show();
}

void readColor() {
  _color[0] = EEPROM.read(EEPROM_COLOR);
  _color[1] = EEPROM.read(EEPROM_COLOR + 1);
  _color[2] = EEPROM.read(EEPROM_COLOR + 2);
}

byte getTimer() {
  byte result = EEPROM.read(EEPROM_TIMER);

  if (result == 255) {
    result = 2;
  }

  return result;
}

byte getAnimation() {
  byte result = EEPROM.read(EEPROM_ANIMATION);

  if (result == 255) {
    result = 0;
  }

  return result;
}

void saveColor() {
  EEPROM.update(EEPROM_COLOR, _color[0]);
  EEPROM.update(EEPROM_COLOR + 1, _color[1]);
  EEPROM.update(EEPROM_COLOR + 2, _color[2]);
}

void saveTimer(byte value) {
  EEPROM.update(EEPROM_TIMER, value);
}

void saveAnimation(byte value) {
  EEPROM.update(EEPROM_ANIMATION, value);
}

String parseCommand(String data, char separator, int index)
{
  int found = 0;
  int strIndex[] = {0, -1};
  int maxIndex = data.length() - 1;

  for (int i = 0; i <= maxIndex && found <= index; i++) {
    if (data.charAt(i) == separator || i == maxIndex || data.charAt(i) == '\n' || data.charAt(i) == '\r') {
      found++;
      strIndex[0] = strIndex[1] + 1;
      strIndex[1] = (i == maxIndex) ? i + 1 : i;
    }
  }

  return found > index ? data.substring(strIndex[0], strIndex[1]) : "";
}

void initAnimation() {
  _animationTimer = millis();
  _animationCpt = 0;
  _animationStep = 0;
}

void animation1() {
  if (millis() - _animationTimer >= 1000UL) {
    _animationTimer = millis();
    leds[random(NUM_LEDS - 1)].setHue( random(255));
    FastLED.show();
  }
}

void animation2() {
  if (millis() - _animationTimer >= 10000UL) {
    _animationTimer = millis();

    for (int i = 0; i < NUM_LEDS; i++) {
      leds[i].setHue( random(255));
    }
    
    FastLED.show();
  }
}
