// Enable debug prints to serial monitor
#define MY_DEBUG
#define LED_ENABLE

#define MY_RADIO_RF24
#define MY_RF24_PA_LEVEL (RF24_PA_MAX)

#include <MySensors.h>

#define PIR_PIN 3
#define LED_PIN A2
#define CAM_POWER_PIN A1
#define CAM_STATUS_PIN A0

MyMessage msg(0, V_TRIPPED);
unsigned long startCamTime = 0;
bool oldTripped = false;

void before()
{
  pinMode(PIR_PIN, INPUT);
  pinMode(CAM_STATUS_PIN, INPUT);
  pinMode(LED_PIN, OUTPUT);
  
  digitalWrite(LED_PIN, HIGH);
}

void setup()
{
  for (char i = 0; i < 10; i++) {
    if (i % 2 == 0) {
      digitalWrite(LED_PIN, HIGH);
    } else {
      digitalWrite(LED_PIN, LOW);
    }

    sleep(100);
  }

  pinMode(LED_PIN, INPUT);
}

void presentation()
{
  sendSketchInfo("PIR with camera", "1.0");
  present(0, S_MOTION);
}

void loop()
{
  bool tripped = digitalRead(PIR_PIN) == HIGH;

#ifdef LED_ENABLE
  if (tripped && !oldTripped) {
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, HIGH);
  } else if (!tripped && oldTripped) {
    digitalWrite(LED_PIN, LOW);
    pinMode(LED_PIN, INPUT);
  }
#endif

  if (tripped != oldTripped) {
    sendWithRetry(msg.set(tripped?"1":"0"), 10);
  }

  oldTripped = tripped;

  if (!tripped && millis() - startCamTime > 11000UL && digitalRead(PIR_PIN) == LOW) {
    stopCam();
    sleep(digitalPinToInterrupt(PIR_PIN), HIGH, 0);
    startCam();
    startCamTime = millis();
  }
}

void startCam() {
  pinMode(CAM_POWER_PIN, OUTPUT);
  digitalWrite(CAM_POWER_PIN, HIGH);
}

void stopCam() {
  digitalWrite(CAM_POWER_PIN, LOW);
  pinMode(CAM_POWER_PIN, INPUT);
}

boolean sendWithRetry(MyMessage &message, const byte retryNumber) {
  byte counter = retryNumber;
  bool success = false;

  do {
    success = send(message, true);

    if (success) {
      success = wait(500, message.getCommand(), message.type);
    }

    if (!success && counter != 0 && (retryNumber - counter) > 0) {
      delay(50 * (retryNumber - counter));
    }
  } while (!success && counter--);

  return success;
}
