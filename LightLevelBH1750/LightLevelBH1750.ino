//#define MY_DEBUG
#define MY_RADIO_RF24

#if defined(MY_DEBUG)
#define DEBUG_PRINT(str) Serial.println(str);
#else
#define DEBUG_PRINT(str)
#endif

#include <MySensors.h>
#include <BH1750.h>
#include <Wire.h>

BH1750 lightSensor;
MyMessage msg(0, V_LEVEL);
uint16_t lastlux = 65535;
int heartbeatCpt = 0;

void setup() {
  heartbeatCpt = 0;
  Wire.begin();
  lightSensor.begin(BH1750::ONE_TIME_HIGH_RES_MODE);
}

void presentation() {
  sendSketchInfo("BH1750 Light Level", "1.0");
  present(0, S_LIGHT_LEVEL);
}

void loop() {
  while (!lightSensor.measurementReady(true)) {
    yield();
  }
  
  uint16_t lux = lightSensor.readLightLevel(); // Get Lux value
  DEBUG_PRINT(lux);
  bool hasChanged = false;

  if (lux >= 500) {
    hasChanged = lastlux < 500;                  // lux > 500
  } else if (lux >= 100) {
    hasChanged = lastlux > 500 || lastlux < 100; // lux > 100
  } else if (lux >= 50) {
    hasChanged = lastlux > 100 || lastlux < 50;  // lux > 50
  } else if (lux >= 25) {
    hasChanged = lastlux > 50 || lastlux < 25;   // lux > 25
  } else if (lux >= 10) {
    hasChanged = lastlux > 25 || lastlux < 10;   // lux > 10
  } else {
    hasChanged = lux != lastlux;                 // lux > 0
  }

  lastlux = lux;

  if (hasChanged) {
    sendWithRetry(msg.set(lux), 10);
  }

  if (heartbeatCpt > 50) {
    sendHeartbeat();
    heartbeatCpt = 0;
  }

  sleep(60000);  // 1 minute
  heartbeatCpt++;
  lightSensor.configure(BH1750::ONE_TIME_HIGH_RES_MODE);
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
