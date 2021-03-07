// Enable debug prints to serial monitor
#define MY_DEBUG

// Enable and select radio type attached
#define MY_RADIO_RF24

// Enable IRQ pin
#define MY_RX_MESSAGE_BUFFER_FEATURE
#define MY_RF24_IRQ_PIN (2)
#define MY_RX_MESSAGE_BUFFER_SIZE (5)

// Enable hardware signing
#define MY_SIGNING_ATSHA204
#define MY_SIGNING_REQUEST_SIGNATURES

#include <MySensors.h>
#include <Parser.h>
#include <Adafruit_Fingerprint.h>

#define FINGER_RX 4
#define FINGER_TX 5
#define FINGER_WAKEUP 3
#define FINGER_POWER 6

Parser parser = Parser(' ');
SoftwareSerial fingerSerial(FINGER_TX, FINGER_RX);
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&fingerSerial);
MyMessage msg(0, V_CUSTOM);

unsigned long _timer = 0;
bool successMsgReceived;
bool configMode = false;

void before()
{
  pinMode(FINGER_WAKEUP, INPUT);
}

void setup() {
  configMode = false;
  startFingerPrint();
  finger.begin(57600);
  delay(5);

  if (finger.verifyPassword()) {
    send(msg.set(F("fingerprint ok")));
    finger.getTemplateCount();
    String templates = "Templates: " + String(finger.templateCount);
    send(msg.set(templates.c_str()));
  } else {
    send(msg.set(F("fingerprint not found")));
  }
}

void presentation() {
  sendSketchInfo("Fingerprint Alarm", "1.0");
  present(0, S_CUSTOM);
}

void receive(const MyMessage &message)
{
  if (message.type == V_CUSTOM && message.sensor == 0 && !message.isAck()) {
    parser.parse(message.getString());

    if (parser.isEqual(0, "success")) {
      successMsgReceived = true;
    } else if (parser.isEqual(0, "enroll")) {
      int id = parser.getInt(1);

      if (id > 0 && id < 128) {
        startFingerPrint();
        enrollFingerprint(id);
        stopFingerPrint();
      } else {
        send(msg.set(F("wrong id")));
      }
    } else if (parser.isEqual(0, "template")) {
      startFingerPrint();
      finger.getTemplateCount();
      String templates = "Templates: " + String(finger.templateCount);
      send(msg.set(templates.c_str()));
      stopFingerPrint();
    } else if (parser.isEqual(0, "delete-all")) {
      startFingerPrint();
      finger.emptyDatabase();
      send(msg.set(F("database empty")));
      stopFingerPrint();
    } else if (parser.isEqual(0, "start-config")) {
      configMode = true;
      _timer = millis();
      send(msg.set(F("start config mode")));
    } else if (parser.isEqual(0, "stop-config")) {
      configMode = false;
      send(msg.set(F("stop config mode")));
    }
  }
}

void loop() {
  if (configMode) {
    if (millis() - _timer > 180000UL) { // 3 minutes
      configMode = false;
    } else {
      wait(100);
    }
  } else if (digitalRead(FINGER_WAKEUP) == HIGH && millis() - _timer > 500UL) {
    stopFingerPrint();
    sleep(digitalPinToInterrupt(FINGER_WAKEUP), LOW, 0);
    startFingerPrint();
    _timer = millis();
  } else {
    checkFingerprint();
    delay(50);

    if (digitalRead(FINGER_WAKEUP) == LOW) {
      _timer = millis();
    }
  }
}

void foundMatch() {
  finger.LEDcontrol(FINGERPRINT_LED_ON, 0, FINGERPRINT_LED_BLUE);
  successMsgReceived = false;
  String message = "match-" + String(finger.fingerID);

  if (sendWithRetry(msg.set(message.c_str()), 10)) {
    wait(500, msg.getCommand(), msg.type);

    if (successMsgReceived) {
      finger.LEDcontrol(FINGERPRINT_LED_FLASHING, 25, FINGERPRINT_LED_BLUE, 10);
      delay(2000);
      return;
    }
  }

  finger.LEDcontrol(FINGERPRINT_LED_FLASHING, 25, FINGERPRINT_LED_RED, 10);
  delay(2000);
}

void noMatch() {
  finger.LEDcontrol(FINGERPRINT_LED_FLASHING, 25, FINGERPRINT_LED_RED, 10);
  send(msg.set(F("no match")));
  wait(2000);
}

uint8_t checkFingerprint() {
  uint8_t p;

  // get image
  p = finger.getImage();
  if (p != FINGERPRINT_OK) {
    sendFingerError(p);
    return false;
  }

  // compress image
  p = finger.image2Tz();
  if (p != FINGERPRINT_OK) {
    sendFingerError(p);
    return false;
  }

  // check match
  p = finger.fingerSearch();
  if (p == FINGERPRINT_OK) {
    foundMatch();
  } else if (p == FINGERPRINT_NOTFOUND) {
    noMatch();
  } else {
    sendFingerError(p);
    return false;
  }

  return true;
}

void startFingerPrint() {
  pinMode(FINGER_POWER, OUTPUT);
  digitalWrite(FINGER_POWER, LOW);
  pinMode(FINGER_RX, OUTPUT);
  fingerSerial.println("a"); // fix first send
  delay(30);
}

void stopFingerPrint() {
  pinMode(FINGER_RX, INPUT);  // because fingerSerial.end() is not working
  pinMode(FINGER_POWER, INPUT);
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

boolean enrollFingerprint(uint8_t id) {
  uint8_t p = -1;

  send(msg.set(F("Waiting for finger")));
  finger.LEDcontrol(FINGERPRINT_LED_ON, 0, FINGERPRINT_LED_PURPLE);

  // get image
  _timer = millis();
  while (p != FINGERPRINT_OK && millis() - _timer < 30000UL) {
    p = finger.getImage();
  }
  if (p != FINGERPRINT_OK) {
    sendFingerErrorAndFlash(p);
    return false;
  }

  // compress image
  p = finger.image2Tz(1);
  if (p != FINGERPRINT_OK) {
    sendFingerErrorAndFlash(p);
    return false;
  }

  finger.LEDcontrol(FINGERPRINT_LED_FLASHING, 25, FINGERPRINT_LED_BLUE, 10);
  send(msg.set(F("Remove finger")));
  delay(2000);
  p = 0;

  _timer = millis();
  while (p != FINGERPRINT_NOFINGER && millis() - _timer < 30000UL) {
    p = finger.getImage();
  }
  if (p != FINGERPRINT_NOFINGER) {
    return false;
  }

  p = -1;
  send(msg.set(F("Place same finger again")));
  finger.LEDcontrol(FINGERPRINT_LED_ON, 0, FINGERPRINT_LED_PURPLE);

  // get image
  _timer = millis();
  while (p != FINGERPRINT_OK && millis() - _timer < 30000UL) {
    p = finger.getImage();
  }
  if (p != FINGERPRINT_OK) {
    sendFingerErrorAndFlash(p);
    return false;
  }

  // compress image
  p = finger.image2Tz(2);
  if (p != FINGERPRINT_OK) {
    sendFingerErrorAndFlash(p);
    return false;
  }

  // create model
  p = finger.createModel();
  if (p != FINGERPRINT_OK) {
    sendFingerErrorAndFlash(p);
    return false;
  }

  // store model
  p = finger.storeModel(id);
  if (p != FINGERPRINT_OK) {
    sendFingerErrorAndFlash(p);
    return false;
  }

  finger.LEDcontrol(FINGERPRINT_LED_FLASHING, 25, FINGERPRINT_LED_BLUE, 10);
  send(msg.set(F("Stored with success!")));
  delay(2000);

  return true;
}

void sendFingerErrorAndFlash(uint8_t error) {
  finger.LEDcontrol(FINGERPRINT_LED_FLASHING, 25, FINGERPRINT_LED_RED, 10);
  sendFingerError(error);
  delay(2000);
}

void sendFingerError(uint8_t error) {
  switch (error) {
    case FINGERPRINT_IMAGEMESS:
      send(msg.set(F("Image too messy")));
      break;
    case FINGERPRINT_FEATUREFAIL:
      send(msg.set(F("Features fail")));
      break;
    case FINGERPRINT_INVALIDIMAGE:
      send(msg.set(F("Invalid image")));
      break;
    case FINGERPRINT_PACKETRECIEVEERR:
      send(msg.set(F("Communication error")));
      break;
    case FINGERPRINT_BADLOCATION:
      send(msg.set(F("Bad location")));
      break;
    case FINGERPRINT_IMAGEFAIL:
      send(msg.set(F("Imaging error")));
      break;
    case FINGERPRINT_FLASHERR:
      send(msg.set(F("Error writing to flash")));
      break;
    case FINGERPRINT_ENROLLMISMATCH:
      send(msg.set(F("Finger did not match")));
      break;
    case FINGERPRINT_NOFINGER:
      break;
    default:
      send(msg.set(F("Unknown error")));
      break;
  }
}
