// Enable debug prints to serial monitor
#define MY_DEBUG
#define MY_BAUD_RATE (9600ul)

#define MY_RADIO_RF24
#define MY_RF24_PA_LEVEL (RF24_PA_MAX)
#define MY_TRANSPORT_MAX_TX_FAILURES (3u)

#define EEPROM_VOLTAGE_CORRECTION EEPROM_LOCAL_CONFIG_ADDRESS + 0 // 4 bytes storage

// Debug print
#if defined(MY_DEBUG)
#define DEBUG_PRINT(str) Serial.println(str);
#else
#define DEBUG_PRINT(str)
#endif

#include <MySensors.h>
#include <Wire.h>
#include "DFRobot_SHT20.h"
#include <BatteryLevel.h>

#define CHILD_ID_DOOR 0
#define CHILD_ID_HUM  1
#define CHILD_ID_TEMP 2

#define DOOR_PIN 2

MyMessage msgDoor(CHILD_ID_DOOR, V_TRIPPED);
MyMessage msgHum( CHILD_ID_HUM,  V_HUM);
MyMessage msgTemp(CHILD_ID_TEMP, V_TEMP);
DFRobot_SHT20 sht20;
BatteryLevel battery(INTERNAL_MEASUREMENT, EEPROM_VOLTAGE_CORRECTION, CR2032_LITHIUM);
int batteryPercent = 101;
int8_t wakeup = MY_WAKE_UP_BY_TIMER;
uint8_t sentValue = 2;

void before()
{
  pinMode(DOOR_PIN, INPUT);

  sht20.initSHT20();
  delay(100);

  //battery.saveVoltageCorrection(0.9874172185430464); // Measured by multimeter divided by reported (with voltage correction = 1.0)
  battery.init();
}

void setup()
{

}

void presentation()
{
  sendSketchInfo("Door", "2.0");
  present(CHILD_ID_DOOR, S_DOOR, "Door");
  present(CHILD_ID_HUM, S_HUM, "Humidity");
  present(CHILD_ID_TEMP, S_TEMP, "Temperature");
}

void receive(const MyMessage &myMsg)
{
  DEBUG_PRINT("message received");
}

void loop()
{
  if (wakeup != MY_WAKE_UP_BY_TIMER) {
    delay(50); // Short delay to allow buttons to properly settle
    uint8_t tripped = digitalRead(DOOR_PIN);

    if (tripped != sentValue) {
      DEBUG_PRINT("tripped");
      sendWithRetry(msgDoor.set(tripped == HIGH), 10);
      sentValue = tripped;
    }
  } else {
    const float temperature = sht20.readTemperature();
    const float humidity = sht20.readHumidity();
    send(msgTemp.set(temperature, 1));
    send(msgHum.set(humidity, 0));
    
    battery.compute();
    //String voltageMsg = "voltage-" + String(battery.getVoltage()) + "-" + String(battery.getPercent());
    //Serial.println(msgDoor.set(voltageMsg.c_str()));
    if (battery.getPercent() < batteryPercent) {
      sendBatteryLevel(battery.getPercent());
      batteryPercent = battery.getPercent();
    }
  }

  if (digitalRead(DOOR_PIN) == sentValue) {
    wakeup = sleep(digitalPinToInterrupt(DOOR_PIN), CHANGE, 600000);
  } else {
    wakeup = MY_SLEEP_NOT_POSSIBLE;
  }
}

void sendWithRetry(MyMessage &message, const byte retryNumber) {
  byte counter = retryNumber;
  bool success = false;

  do {
    DEBUG_PRINT("send message");
    success = send(message, true);

    if (success) {
      success = wait(500, message.getCommand(), message.type);

      if (!success) {
        DEBUG_PRINT("no software ACK");
      }
    } else {
      DEBUG_PRINT("no hardware ACK");
    }

    if (!success && counter != 0 && (retryNumber - counter) > 0) {
      delay(500 * (retryNumber - counter));
    }
  } while (!success && counter--);
}
