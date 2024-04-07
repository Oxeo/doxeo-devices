#include "Arduino.h"
#include <SoftwareSerial.h>

#define BLE_RX_PIN 5
#define BLE_TX_PIN 6

SoftwareSerial ble(BLE_TX_PIN, BLE_RX_PIN); // RX, TX

void setup() {
  // Change baud to 9600
  ble.begin(115200);
  delay(1000);
  ble.print("AT+BAUD=2");
  delay(1000);

  Serial.begin(9600);
  ble.begin(9600);
  delay(1000);

  // Transparent transmission
  ble.print("AT+TRANMD=1"); 
  delay(1000);

  // Device name
  ble.print("AT+NAME=[onl] test1"); 
}

void loop() {
  while (ble.available()) {
    String msg = ble.readStringUntil('\n');
    Serial.println("ble: " + msg);
  }

  if (Serial.available() > 0) {
    char incomingByte = Serial.read();
    ble.write(incomingByte);
  }
}
