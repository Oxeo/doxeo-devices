#include <LowPower.h>
#include <RCSwitch.h>

//#define DEBUG
#include "DebugUtils.h"

#define RF 7
#define SENSOR 3

enum Status {open, close, unknown};

RCSwitch rcSwitch = RCSwitch();
Status sensorStatus = unknown;

void setup() {
  // init PIN
  pinMode(RF, OUTPUT);
  pinMode(SENSOR, INPUT);
  pinMode(2, INPUT);
  pinMode(10, INPUT);
  pinMode(LED_BUILTIN, OUTPUT);
  
  rcSwitch.enableTransmit(RF);

  // init serial for debugging
#ifdef DEBUG
  Serial.begin(9600);
  Serial.println("debug mode enable");
#endif

  sensorStatus = unknown;

  for (char i=0; i<50; i++) {
    if (i%2 == 0) {
      digitalWrite(LED_BUILTIN, HIGH);
    } else {
      digitalWrite(LED_BUILTIN, LOW);
    }

    delay(50);
  }

  pinMode(LED_BUILTIN, INPUT);
}


void loop() {
  pinMode(SENSOR, INPUT_PULLUP);

  if (digitalRead(SENSOR) == HIGH && sensorStatus != open) {
    //digitalWrite(LED_BUILTIN, HIGH);
    sensorStatus = open;
    pinMode(RF, OUTPUT);
    rcSwitch.send(1283551, 24);
    pinMode(RF, INPUT);
  } else if (digitalRead(SENSOR) == LOW && sensorStatus != close) {
    //digitalWrite(LED_BUILTIN, LOW);
    sensorStatus = close;
    pinMode(RF, OUTPUT);
    rcSwitch.send(1283041, 24);
    pinMode(RF, INPUT);
  }

  //sleepForever();
  pinMode(SENSOR, INPUT);
  LowPower.powerDown(SLEEP_2S, ADC_OFF, BOD_OFF);
}

void sleepForever() {
  attachInterrupt(digitalPinToInterrupt(SENSOR), sensorInterruptHandle, CHANGE);
  LowPower.powerDown(SLEEP_FOREVER, ADC_OFF, BOD_OFF);
  detachInterrupt(digitalPinToInterrupt(SENSOR));
}

void sensorInterruptHandle()
{
  // nothing to do
}

