#include "Arduino.h"
#include <EEPROM.h>
#include <SoftwareSerial.h>
#include <FastLED.h>
#include <DFRobotDFPlayerMini.h>
#include <AccelStepper.h>

#define BLE_RX_PIN 5
#define BLE_TX_PIN 6
#define BLE_LINK_PIN 7
#define BLE_DATA_PIN 2
#define BLE_CMD_PIN A0

#define DFPLAYER_BUSY A1
#define DFPLAYER_RX_PIN 8
#define DFPLAYER_TX_PIN 4

#define MOTOR_IN1_PIN 10
#define MOTOR_IN2_PIN 11
#define MOTOR_IN3_PIN 12
#define MOTOR_IN4_PIN 13
#define MOTOR_INTERFACE_TYPE 8

#define BUTTON_PIN 3
#define LED_PIN 9

#define NUM_LEDS 10

#define EEPROM_COLOR 0 // 3 byte
#define EEPROM_VOLUME 4 // 1 byte
#define EEPROM_MUSIC 5 // 1 byte
#define EEPROM_TIMER 6 // 1 byte

SoftwareSerial ble(BLE_TX_PIN, BLE_RX_PIN);

CRGB leds[NUM_LEDS];

SoftwareSerial dfPlayerSerial(DFPLAYER_TX_PIN, DFPLAYER_RX_PIN);
DFRobotDFPlayerMini dfPlayer;

AccelStepper motor = AccelStepper(MOTOR_INTERFACE_TYPE, MOTOR_IN1_PIN, MOTOR_IN3_PIN, MOTOR_IN2_PIN, MOTOR_IN4_PIN);

byte _volume;
boolean _animationIsOn = false;
byte _color[3];

unsigned long _startTime = 0;
unsigned long _timer;

int _music = 0;
unsigned long _animationTimer = 0;
byte _fadeLight = 0;
boolean _fadeLightReverse = false;

void setup() {
  randomSeed(analogRead(0));
  Serial.begin(9600);

  Serial.println(F("Starting..."));

  pinMode(BLE_CMD_PIN, OUTPUT);
  pinMode(BLE_DATA_PIN, INPUT);
  pinMode(BLE_LINK_PIN, INPUT);
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT);
  pinMode(DFPLAYER_BUSY, INPUT);

  ble.begin(9600);

  FastLED.addLeds<WS2812, LED_PIN, GRB>(leds, NUM_LEDS);

  dfPlayerSerial.begin(9600);

  if (!dfPlayer.begin(dfPlayerSerial)) {
    Serial.println(F("Unable to init DFPlayer:"));
    Serial.println(F("1.Please recheck the connection!"));
    Serial.println(F("2.Please insert the SD card!"));
    while (true);
  }

  Serial.println(F("DFPlayer Mini online"));

  motor.setMaxSpeed(1000);
  motor.setSpeed(100);

  _volume = getVolume();
  _music = getMusic();
  _timer = getTimer();
  readColor();

  Serial.println("Volume: " + String(_volume));
  Serial.println("Music: " + String(_music));
  Serial.println("Timer: " + String(_timer) + " minutes");

  changeVolume(_volume);
  //dfPlayer.play(1);

  for (int i = 0; i < NUM_LEDS; i++) {
    leds[i] = CRGB ( 0, 0, 255);
    FastLED.show();
  }

  delay(500);

  for (int i = (NUM_LEDS - 1) ; i >= 0; i--) {
    leds[i] = CRGB (0, 0, 0);
    FastLED.show();
  }

  if (digitalRead(BLE_LINK_PIN) == HIGH) {
    sendDataToBleDevice();
  }

  Serial.println(F("Started"));
}

void loop() {
  ble.listen();

  while (ble.available()) {
    String msg = ble.readStringUntil('\n');
    Serial.println("ble: " + msg);

    if (msg == "cmd+start") {
      ble.println("status:on");
      startAnimation();
    } else if (msg == "cmd+stop") {
      ble.println("status:off");
      stopAnimation();
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

      if (!_animationIsOn) {
        stopAnimation();
      }
    } else if (msg.startsWith("cmd+volume=")) {
      byte volume = parseCommand(msg, '=', 1).toInt();
      saveVolume(volume);
      Serial.println("Volume: " + String(volume));
      ble.println("volume:" + String(volume));
      changeVolume(volume);
    } else if (msg.startsWith("cmd+music=")) {
      byte music = parseCommand(msg, '=', 1).toInt();

      if (music >= 1 && music <= 255) {
        saveMusic(music);
        _music = music;
        Serial.println("Music: " + String(music));
        ble.println("music:" + String(music));

        if (_animationIsOn) {
          stopAnimation();
          startAnimation();
        }
      }
    } else if (msg.startsWith("cmd+timer=")) {
      byte timer = parseCommand(msg, '=', 1).toInt();
      saveTimer(timer);
      Serial.println("Timer: " + String(timer));
      ble.println("timer:" + String(timer));
    } else if (msg.startsWith("cmd+name=")) {
      String name = parseCommand(msg, '=', 1);
      Serial.println("Name: " + String(name));
      ble.print("+++");
      delay(500);
      ble.print("AT+NAME=[omc] " + name);
      delay(500);
      ble.print("AT+EXIT");
    }
  }

  if (Serial.available() > 0) {
    char incomingByte = Serial.read();
    ble.write(incomingByte);
  }

  if (_animationIsOn) {
    if (millis() - _animationTimer >= 20UL) {
      _animationTimer = millis();

      for (int i = 1; i < NUM_LEDS; i++) {
        leds[i] = CRGB (_color[0], _color[1], _color[2]);
        leds[i].fadeLightBy(_fadeLight);
      }

      if (_fadeLightReverse) {
        _fadeLight--;
      } else {
        _fadeLight++;
      }

      if (_fadeLight == 0 || _fadeLight == 220) {
        _fadeLightReverse = !_fadeLightReverse;
      }

      FastLED.show();
    }

    motor.runSpeed();

    /*dfPlayerSerial.listen();
      if (dfPlayer.available()) {
      byte status = dfPlayer.readType();

      // Play finished
      if (status == DFPlayerPlayFinished) {
       stopAnimation();
      }
      }*/

    //digitalRead(DFPLAYER_BUSY) == LOW ||
    if ( (_timer != 0 && millis() - _startTime >= _timer * 60000UL)) {
      stopAnimation();
    }
  }

  if (digitalRead(BUTTON_PIN) == LOW) {
    if (_animationIsOn) {
      stopAnimation();
    } else {
      startAnimation();
    }

    if (digitalRead(BLE_LINK_PIN) == HIGH) {
      sendDataToBleDevice();
    }

    delay(500);
  }
}

void startAnimation() {
  dfPlayer.playFolder(1, _music);

  for (int i = 0; i < NUM_LEDS; i++) {
    leds[i] = CRGB (_color[0], _color[1], _color[2]);
    FastLED.show();
  }

  _animationTimer = 0;
  _startTime = millis();
  _fadeLight = 0;
  _fadeLightReverse = false;
  _animationIsOn = true;

  Serial.println(F("Animation started"));
}

void stopAnimation() {
  dfPlayer.pause();

  for (int i = (NUM_LEDS - 1) ; i >= 0; i--) {
    leds[i] = CRGB (0, 0, 0);
    FastLED.show();
    delay(10);
  }

  _animationIsOn = false;
  Serial.println(F("Animation stopped"));
}

void changeVolume(byte volume) {
  byte a = map(volume, 0, 100, 0, 30);
  dfPlayer.volume(a);
  Serial.println(a);
}

void sendDataToBleDevice() {
  String status = _animationIsOn ? "on" : "off";
  String hexstring = "";

  for (int i = 0; i < 3; i++) {
    if (_color[i] < 16) {
      hexstring += "0" + String(_color[i], HEX);
    } else {
      hexstring += String(_color[i], HEX);
    }
  }

  ble.println("status:" + status);
  ble.println("color:" + hexstring);
  ble.println("volume:" + String(_volume));
  ble.println("music:" + String(_music));
  ble.println("timer:" + String(_timer));
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

byte getVolume() {
  byte result = EEPROM.read(EEPROM_VOLUME);

  if (result == 255) {
    result = 30;
  }

  return result;
}

byte getMusic() {
  byte result = EEPROM.read(EEPROM_MUSIC);

  if (result == 255) {
    result = 1;
  }

  return result;
}

byte getTimer() {
  byte result = EEPROM.read(EEPROM_TIMER);

  if (result == 255) {
    result = 10;
  }

  return result;
}

void saveColor() {
  EEPROM.update(EEPROM_COLOR, _color[0]);
  EEPROM.update(EEPROM_COLOR + 1, _color[1]);
  EEPROM.update(EEPROM_COLOR + 2, _color[2]);
}

void saveVolume(byte value) {
  EEPROM.update(EEPROM_VOLUME, value);
  _volume = value;
}

void saveMusic(byte value) {
  EEPROM.update(EEPROM_MUSIC, value);
  _music = value;
}

void saveTimer(byte value) {
  EEPROM.update(EEPROM_TIMER, value);
  _timer = value;
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
