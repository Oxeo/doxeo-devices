#define MY_DEBUG
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

MyMessage _customMsg(0, V_CUSTOM);
Parser _parser = Parser(' ');
SoftwareSerial _fingerSerial(FINGER_TX, FINGER_RX);
Adafruit_Fingerprint _finger = Adafruit_Fingerprint(&_fingerSerial);
unsigned long _timer = 0;
bool _successMsgReceived = false;
bool _configMode = false;

void before()
{
  pinMode(FINGER_WAKEUP, INPUT);
  pinMode(FINGER_POWER, INPUT);
}

void setup() {
  _configMode = false;
  startFingerPrint();
  _finger.begin(57600);
  delay(5);

  if (_finger.verifyPassword()) {
    send(_customMsg.set(F("fingerprint ok")));
    _finger.getTemplateCount();
    String templates = "Templates: " + String(_finger.templateCount);
    send(_customMsg.set(templates.c_str()));
  } else {
    send(_customMsg.set(F("fingerprint not found")));
  }
}

void presentation() {
  sendSketchInfo("Fingerprint Alarm", "1.0");
  present(0, S_CUSTOM, "Fingerprint");
}

void receive(const MyMessage &message)
{
  if (message.type == V_CUSTOM && message.sensor == 0 && !message.isAck()) {
    _parser.parse(message.getString());

    if (_parser.isEqual(0, "success")) {
      _successMsgReceived = true;
    } else if (_parser.isEqual(0, "enroll")) {
      uint8_t id = 0;

      if (_parser.get(1) != NULL) {
        id = _parser.getInt(1);
      } else {
        _finger.getTemplateCount();
        id = _finger.templateCount + 1;
      }

      startFingerPrint();
      enrollFingerprintWithMessage(id);
      stopFingerPrint();
    } else if (_parser.isEqual(0, "template")) {
      startFingerPrint();
      _finger.getTemplateCount();
      String templates = "Templates: " + String(_finger.templateCount);
      send(_customMsg.set(templates.c_str()));
      stopFingerPrint();
    } else if (_parser.isEqual(0, "delete-all")) {
      startFingerPrint();
      _finger.emptyDatabase();
      enrollFingerprintWithMessage(1);
      send(_customMsg.set(F("database erased")));
      stopFingerPrint();
    } else if (_parser.isEqual(0, "start-config")) {
      _configMode = true;
      _timer = millis();
      send(_customMsg.set(F("start config mode")));
    } else if (_parser.isEqual(0, "stop-config")) {
      _configMode = false;
      send(_customMsg.set(F("stop config mode")));
    }
  }
}

void loop() {
  if (_configMode) {
    if (millis() - _timer > 300000UL) { // 5 minutes
      _configMode = false;
    } else {
      wait(100);
    }
  } else if (digitalRead(FINGER_WAKEUP) == HIGH && millis() - _timer > 500UL) {
    stopFingerPrint();
    sleep(digitalPinToInterrupt(FINGER_WAKEUP), LOW, 0);
    startFingerPrint();
    _timer = millis();
  } else {
    uint8_t result = checkFingerprint();

    if (result == FINGERPRINT_OK) {
      foundMatch();

      if (_finger.fingerID == 1) {
        _finger.getTemplateCount();
        enrollFingerprintWithMessage(_finger.templateCount + 1);
      }
    } else if (result == FINGERPRINT_NOTFOUND) {
      noMatch();
    } else if (result != FINGERPRINT_NOFINGER) {
      sendFingerError(result);
    }

    delay(50);

    if (digitalRead(FINGER_WAKEUP) == LOW) {
      _timer = millis();
    }
  }
}

void foundMatch() {
  _finger.LEDcontrol(FINGERPRINT_LED_ON, 0, FINGERPRINT_LED_BLUE);
  _successMsgReceived = false;
  String message = "match-" + String(_finger.fingerID);

  if (sendWithRetry(_customMsg.set(message.c_str()), 10)) {
    wait(500, _customMsg.getCommand(), _customMsg.type);

    if (_successMsgReceived) {
      delay(300);
      send(_customMsg.set(F("released")));
      delay(500);

      _finger.LEDcontrol(FINGERPRINT_LED_FLASHING, 25, FINGERPRINT_LED_BLUE, 10);
      wait(2000);
      return;
    }
  }

  delay(300);
  send(_customMsg.set(F("released")));
  delay(500);

  _finger.LEDcontrol(FINGERPRINT_LED_FLASHING, 25, FINGERPRINT_LED_RED, 10);
  wait(2000);
}

void noMatch() {
  _finger.LEDcontrol(FINGERPRINT_LED_FLASHING, 25, FINGERPRINT_LED_RED, 10);
  send(_customMsg.set(F("no match")));
  delay(300);
  send(_customMsg.set(F("released")));
  wait(2000);
}

uint8_t checkFingerprint() {
  // get image
  uint8_t p = _finger.getImage();
  if (p != FINGERPRINT_OK) {
    return p;
  }

  // compress image
  p = _finger.image2Tz();
  if (p != FINGERPRINT_OK) {
    return p;
  }

  // check match
  return _finger.fingerSearch();
}

void startFingerPrint() {
  pinMode(FINGER_POWER, OUTPUT);
  digitalWrite(FINGER_POWER, LOW);
  pinMode(FINGER_RX, OUTPUT);
  _fingerSerial.println("a"); // fix first send
  delay(30);
}

void stopFingerPrint() {
  pinMode(FINGER_RX, INPUT);  // because _fingerSerial.end() is not working
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

boolean enrollFingerprintWithMessage(uint8_t id) {
  if (id < 1 || id > 127) {
    send(_customMsg.set(F("Wrong id")));
    return false;
  }

  String templates = "Start enrolling " + String(id);
  send(_customMsg.set(templates.c_str()));

  uint8_t result = enrollFingerprint(id);

  if (result == FINGERPRINT_OK) {
    String templates = String(id) + F(" stored with success!");
    send(_customMsg.set(templates.c_str()));
    delay(2000);
    return true;
  } else {
    _finger.LEDcontrol(FINGERPRINT_LED_FLASHING, 25, FINGERPRINT_LED_RED, 10);
    sendFingerError(result);
    delay(2000);
    return false;
  }
}

boolean enrollFingerprint(uint8_t id) {
  uint8_t p = FINGERPRINT_PACKETRESPONSEFAIL;
  _finger.LEDcontrol(FINGERPRINT_LED_ON, 0, FINGERPRINT_LED_PURPLE);

  // get image
  _timer = millis();
  do {
    p = _finger.getImage();
  } while (p != FINGERPRINT_OK && millis() - _timer < 30000UL);
  if (p != FINGERPRINT_OK) {
    return p;
  }

  // compress image
  p = _finger.image2Tz(1);
  if (p != FINGERPRINT_OK) {
    return p;
  }

  _finger.LEDcontrol(FINGERPRINT_LED_FLASHING, 25, FINGERPRINT_LED_BLUE, 10);
  delay(2000);

  _timer = millis();
  do {
    p = _finger.getImage();
  } while (p != FINGERPRINT_NOFINGER && millis() - _timer < 30000UL);
  if (p != FINGERPRINT_NOFINGER) {
    return p;
  }

  _finger.LEDcontrol(FINGERPRINT_LED_ON, 0, FINGERPRINT_LED_PURPLE);

  // get image
  _timer = millis();
  do {
    p = _finger.getImage();
  } while (p != FINGERPRINT_OK && millis() - _timer < 30000UL);
  if (p != FINGERPRINT_OK) {
    return p;
  }

  // compress image
  p = _finger.image2Tz(2);
  if (p != FINGERPRINT_OK) {
    return p;
  }

  // create model
  p = _finger.createModel();
  if (p != FINGERPRINT_OK) {
    return p;
  }

  // store model
  p = _finger.storeModel(id);
  if (p != FINGERPRINT_OK) {
    return p;
  }

  _finger.LEDcontrol(FINGERPRINT_LED_FLASHING, 25, FINGERPRINT_LED_BLUE, 10);
  return p;
}

void sendFingerError(uint8_t error) {
  switch (error) {
    case FINGERPRINT_IMAGEMESS:
      send(_customMsg.set(F("Image too messy")));
      break;
    case FINGERPRINT_FEATUREFAIL:
      send(_customMsg.set(F("Features fail")));
      break;
    case FINGERPRINT_INVALIDIMAGE:
      send(_customMsg.set(F("Invalid image")));
      break;
    case FINGERPRINT_PACKETRECIEVEERR:
      send(_customMsg.set(F("Communication error")));
      break;
    case FINGERPRINT_BADLOCATION:
      send(_customMsg.set(F("Bad location")));
      break;
    case FINGERPRINT_IMAGEFAIL:
      send(_customMsg.set(F("Imaging error")));
      break;
    case FINGERPRINT_FLASHERR:
      send(_customMsg.set(F("Error writing to flash")));
      break;
    case FINGERPRINT_ENROLLMISMATCH:
      send(_customMsg.set(F("Finger did not match")));
      break;
    case FINGERPRINT_NOFINGER:
      send(_customMsg.set(F("No finger")));
      break;
    default:
      send(_customMsg.set(F("Unknown error")));
      break;
  }
}
