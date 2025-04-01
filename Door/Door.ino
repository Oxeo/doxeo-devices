// Enable debug prints to serial monitor
//#define MY_DEBUG
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
#include <BatteryLevel.h>

#define DOOR_ID 0
#define DOOR_PIN 2
#define OPEN_STATUS LOW

MyMessage msg(DOOR_ID, V_TRIPPED);
BatteryLevel battery(INTERNAL_MEASUREMENT, EEPROM_VOLTAGE_CORRECTION, CR2032_LITHIUM);
int batteryPercent = 101;
uint8_t sentValue = 2;

void before()
{
  pinMode(DOOR_PIN, INPUT);

  //battery.saveVoltageCorrection(1.014545454545455); // Measured by multimeter divided by reported (with voltage correction = 1.0)
  battery.init();
}

void presentation()
{
  wait(500);
  sendSketchInfo("Door", "2.0");
  wait(500);
  present(DOOR_ID, S_DOOR, "Contact (closed/open)");
}

void receive(const MyMessage &myMsg)
{
  DEBUG_PRINT("message received");
}

void loop()
{
  delay(50); // Short delay to allow buttons to properly settle
  uint8_t tripped = digitalRead(DOOR_PIN);

  if (tripped != sentValue) {
    DEBUG_PRINT("tripped");
    sendWithRetry(msg.set(tripped == OPEN_STATUS), 10);
    sentValue = tripped;
  }

  delay(100);
  battery.compute();
  //String voltageMsg = "voltage-" + String(battery.getVoltage()) + "-" + String(battery.getPercent());
  //Serial.println(voltageMsg);
  if (battery.getPercent() < batteryPercent) {
    sendBatteryLevel(battery.getPercent());
    batteryPercent = battery.getPercent();
  }

  if (digitalRead(DOOR_PIN) == sentValue) {
    sleep(digitalPinToInterrupt(DOOR_PIN), CHANGE, 0);
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
      sleep(500 * (retryNumber - counter));
    }
  } while (!success && counter--);
}
