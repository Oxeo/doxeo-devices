#define MY_DEBUG
#define MY_RADIO_RF24

// Enable IRQ pin
#define MY_RX_MESSAGE_BUFFER_FEATURE
#define MY_RF24_IRQ_PIN (2)

#include <MySensors.h>

#define IR_SENSOR_PIN 3
#define LED_PIN 4

enum state_enum {STOPPED, RUNNING, PENDING};
uint8_t _state;

MyMessage msg(0, V_CUSTOM);
unsigned long irTime = 4000000000UL;
boolean blinkLed = false;
boolean irEvent = false;

void before()
{
  pinMode(IR_SENSOR_PIN, INPUT);
  pinMode(LED_PIN, OUTPUT);
}

void setup()
{
  _state = STOPPED;
  attachInterrupt(digitalPinToInterrupt(IR_SENSOR_PIN), irHandler, CHANGE);
}

void presentation()
{
  sendSketchInfo("3D Printer", "1.0");
  present(0, S_CUSTOM);
}

void loop()
{
  if (irEvent) {
    irTime = millis();
    irEvent = false;
  }
  
  if (millis() - irTime < 30000UL) {
    if (_state != RUNNING) {
      changeState(RUNNING);
    }
  } else if (millis() - irTime < 900000UL) {
    if (_state != PENDING) {
      changeState(PENDING);
    }
  } else {
    if (_state != STOPPED) {
      changeState(STOPPED);
    }
  }

  if (blinkLed) {
    static boolean ledOn = false;
    static unsigned long blinkTimer = 0;
    
    if (millis() - blinkTimer > 1000) {
      blinkTimer = millis();
      ledOn = !ledOn;
      digitalWrite(LED_PIN, ledOn ? LOW : HIGH);
    }
  }
}

void changeState(state_enum state) {
  switch (state) {
    case STOPPED:
      digitalWrite(LED_PIN, LOW);
      blinkLed = false;
      sendWithRetry(msg.set(F("stopped")), 10);
      break;
    case RUNNING:
      digitalWrite(LED_PIN, HIGH);
      blinkLed = false;
      sendWithRetry(msg.set(F("running")), 10);
      break;
    case PENDING:
      blinkLed = true;
      sendWithRetry(msg.set(F("pending")), 10);
      break;
  }

  _state = state;
}

void irHandler() {
  irEvent = true;
}

void sendWithRetry(MyMessage &message, const byte retryNumber) {
  byte counter = retryNumber;
  bool success = false;

  do {
    success = send(message, true);

    if (success) {
      success = wait(500, message.getCommand(), message.type);

      if (!success) {
        Serial.println("no software ACK");
      }
    } else {
      Serial.println("no hardware ACK");
    }
    
    if (!success && counter != 0 && (retryNumber - counter) > 0) {
      sleep(500 * (retryNumber - counter));
    }
  } while (!success && counter--);
}
