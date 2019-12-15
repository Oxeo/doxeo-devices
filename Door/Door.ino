// Enable debug prints to serial monitor
//#define MY_DEBUG
#define MY_BAUD_RATE (9600ul)

#define REPORT_BATTERY_LEVEL

#define MY_RADIO_RF24
#define MY_RF24_PA_LEVEL (RF24_PA_MAX)
#define MY_TRANSPORT_MAX_TX_FAILURES (3u)

// Debug print
#if defined(MY_DEBUG)
#define DEBUG_PRINT(str) Serial.println(str);
#else
#define DEBUG_PRINT(str)
#endif

#include <MySensors.h>

#define DOOR_ID 0
#define DOOR_PIN 2

MyMessage msg(DOOR_ID, V_TRIPPED);

#ifdef REPORT_BATTERY_LEVEL
#include <Vcc.h>
static uint8_t oldBatteryPcnt = 200;  // Initialize to 200 to assure first time value will be sent.
const float VccMin        = 1.8;      // Minimum expected Vcc level, in Volts: Brownout at 1.8V    -> 0%
const float VccMax        = 3.3; // Maximum expected Vcc level, in Volts: 2xAA fresh Alkaline -> 100%
const float VccCorrection = 1.0;      // Measured Vcc by multimeter divided by reported Vcc
static Vcc vcc(VccCorrection);
#endif

void setup()
{
  // Setup the buttons
  pinMode(DOOR_PIN, INPUT);

  // init PIN
  pinMode(A0, INPUT);
  pinMode(8, INPUT);
}

void presentation()
{
  wait(500);
  sendSketchInfo("Door", "1.8");
  wait(500);
  present(DOOR_ID, S_DOOR);
}

void receive(const MyMessage &myMsg)
{
  DEBUG_PRINT("message received");
}

void loop()
{
  uint8_t tripped;
  static uint8_t sentValue = 2;

  // Short delay to allow buttons to properly settle
  delay(50);

  tripped = digitalRead(DOOR_PIN);

  if (tripped != sentValue) {
    DEBUG_PRINT("tripped");
    sendWithRetry(msg.set(tripped == HIGH), 10);
    sentValue = tripped;
  }

#ifdef REPORT_BATTERY_LEVEL
  const uint8_t batteryPcnt = static_cast<uint8_t>(0.5 + vcc.Read_Perc(VccMin, VccMax));

#ifdef MY_DEBUG
  Serial.print(F("Vbat "));
  Serial.print(vcc.Read_Volts());
  Serial.print(F("\tPerc "));
  Serial.println(batteryPcnt);
#endif

  // Battery readout should only go down. So report only when new value is smaller than previous one.
  if ( batteryPcnt < oldBatteryPcnt )
  {
    delay(10);
    sendBatteryLevel(batteryPcnt);
    oldBatteryPcnt = batteryPcnt;
  }
#endif

  if (digitalRead(DOOR_PIN) == sentValue) {
    // Sleep until something happens with the sensor
    sleep(DOOR_PIN - 2, CHANGE, 0);
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
