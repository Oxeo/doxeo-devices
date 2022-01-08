#include "Arduino.h"
#include <LowPower.h>

#define BUTTON1 2
#define LED1 4
#define LED2 5
#define LED3 6
#define LED4 7
#define LED5 8
#define BUZZER 9

int timeSelected = 0;
unsigned long timeToWait[] = {60, 90, 120, 180, 300};
unsigned long timer;
int ledToBlink = 0;
unsigned long ledBlinkTimer = 0;
int ledBlinkState = LOW;

void setup() {
  Serial.begin(115200);
  Serial.print("start init... ");
  pinMode(BUTTON1, INPUT);
  pinMode(LED1, OUTPUT);
  pinMode(LED2, OUTPUT);
  pinMode(LED3, OUTPUT);
  pinMode(LED4, OUTPUT);
  pinMode(LED5, OUTPUT);
  pinMode(BUZZER, OUTPUT);

  attachInterrupt(digitalPinToInterrupt(BUTTON1), button1Handler, CHANGE);
  
  Serial.println("done");
  delay(100);
  
  sleepForever();
}

void loop() {
  // Button pressed
  if (digitalRead(BUTTON1) == LOW) {
    unsigned long pressedTime = millis();
    bool longPress = false;
  
    while (digitalRead(BUTTON1) == LOW) {
      if ((millis() - pressedTime) > 1000) {
        longPress = true;
        Serial.println("button 1 long pressed");
      }
    }
  
    if (longPress == false) {
      Serial.println("button 1 pressed");

      if (timer > 0) {
        timeSelected += 1;

        if (timeSelected > 4) {
          timeSelected = 0;
        }

        Serial.println("Time select to " + String(timeToWait[timeSelected]) + " secondes");
      }
      
      timer = millis();
    }
  }

  // timer running
  if (timer > 0) {
    long remainingTimeS = long ((timer + timeToWait[timeSelected] * 1000 - millis())) / 1000;

    if (remainingTimeS > timeToWait[3]) {
      if (ledToBlink != 5) {
        digitalWrite(LED1, HIGH);  
        digitalWrite(LED2, HIGH);   
        digitalWrite(LED3, HIGH);   
        digitalWrite(LED4, HIGH);   
        digitalWrite(LED5, HIGH);
        ledToBlink = 5;
      }
    } else if (remainingTimeS > timeToWait[2]) {
      if (ledToBlink != 4) {
        digitalWrite(LED1, HIGH);  
        digitalWrite(LED2, HIGH);   
        digitalWrite(LED3, HIGH);   
        digitalWrite(LED4, HIGH);   
        digitalWrite(LED5, LOW);
        ledToBlink = 4;
      }
    } else if (remainingTimeS > timeToWait[1]) {
      if (ledToBlink != 3) {
        digitalWrite(LED1, HIGH);  
        digitalWrite(LED2, HIGH);   
        digitalWrite(LED3, HIGH);   
        digitalWrite(LED4, LOW);   
        digitalWrite(LED5, LOW);
        ledToBlink = 3;
      }
    } else if (remainingTimeS > timeToWait[0]) {
      if (ledToBlink != 2) {
        digitalWrite(LED1, HIGH);  
        digitalWrite(LED2, HIGH);   
        digitalWrite(LED3, LOW);   
        digitalWrite(LED4, LOW);   
        digitalWrite(LED5, LOW); 
        ledToBlink = 2;
      }
    } else {
      if (ledToBlink != 1) {
        digitalWrite(LED1, HIGH);  
        digitalWrite(LED2, LOW);   
        digitalWrite(LED3, LOW);   
        digitalWrite(LED4, LOW);   
        digitalWrite(LED5, LOW);
        ledToBlink = 1;
      }
    }

    // countdown finish
    if (remainingTimeS <= 0) {
      Serial.println("Countdown timer elapsed");
      timer = 0;

      digitalWrite(LED1, LOW);
      ledToBlink = 0;

      for (int i= 0; i<3; i++) {
        digitalWrite(BUZZER, HIGH);
        delay(1000);
        digitalWrite(BUZZER, LOW);
        delay(500);
      }
      
      sleepForever();
    }
  }

  if (ledToBlink > 0) {
     if ((millis() - ledBlinkTimer) > 500) {
      ledBlinkState = ledBlinkState == HIGH ? LOW : HIGH;
      digitalWrite(3+ledToBlink, ledBlinkState);
      ledBlinkTimer = millis();
    }
  }
}

void sleepForever() {
  pinMode(LED1, INPUT);
  pinMode(LED2, INPUT);
  pinMode(LED3, INPUT);
  pinMode(LED4, INPUT);
  pinMode(LED5, INPUT);
  pinMode(BUZZER, INPUT);
  
  Serial.println("sleep forever");
  delay(100);
  
  LowPower.powerDown(SLEEP_FOREVER, ADC_OFF, BOD_OFF);
  
  Serial.println("wake up");
  
  pinMode(LED1, OUTPUT);
  pinMode(LED2, OUTPUT);
  pinMode(LED3, OUTPUT);
  pinMode(LED4, OUTPUT);
  pinMode(LED5, OUTPUT);
  pinMode(BUZZER, OUTPUT);
}

void button1Handler() {
}
