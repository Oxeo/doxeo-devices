#define MY_DEBUG
#define MY_CORE_ONLY

#include <MySensors.h>
#include <Adafruit_Fingerprint.h>

#define FINGER_RX 4
#define FINGER_TX 5
#define FINGER_WAKEUP 3
#define FINGER_POWER 6
#define LOCK 8

SoftwareSerial _fingerSerial(FINGER_TX, FINGER_RX);
Adafruit_Fingerprint _finger = Adafruit_Fingerprint(&_fingerSerial);
unsigned long _timer = 0;

void before()
{
  pinMode(FINGER_WAKEUP, INPUT);
  pinMode(FINGER_POWER, INPUT);
  pinMode(LOCK, INPUT);
}

void setup() {
  startFingerPrint();
  _finger.begin(57600);
  delay(5);

  if (_finger.verifyPassword()) {
    Serial.println(F("fingerprint ok"));
    _finger.getTemplateCount();
    String templates = "Templates: " + String(_finger.templateCount);
    Serial.println(templates.c_str());
  } else {
    Serial.println(F("fingerprint not found"));
  }

  //enrollFingerprintWithMessage(1);
  //_finger.emptyDatabase();
}

void loop() {
  if (digitalRead(FINGER_WAKEUP) == HIGH && millis() - _timer > 500UL) {
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

void openLock() {
  pinMode(LOCK, OUTPUT);
  digitalWrite(LOCK, HIGH);
  delay(800);
  digitalWrite(LOCK, LOW);
  pinMode(LOCK, INPUT);
}

void foundMatch() {
  _finger.LEDcontrol(FINGERPRINT_LED_ON, 0, FINGERPRINT_LED_BLUE);
  openLock();
  _finger.LEDcontrol(FINGERPRINT_LED_FLASHING, 25, FINGERPRINT_LED_BLUE, 10);
  wait(2000);
}

void noMatch() {
  _finger.LEDcontrol(FINGERPRINT_LED_FLASHING, 25, FINGERPRINT_LED_RED, 10);
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
  // because _fingerSerial.end() is not working
  pinMode(FINGER_TX, INPUT);
  pinMode(FINGER_RX, INPUT);
  
  pinMode(FINGER_POWER, INPUT);
}

boolean enrollFingerprintWithMessage(uint8_t id) {
  if (id < 1 || id > 127) {
    Serial.println(F("Wrong id"));
    return false;
  }

  String templates = "Start enrolling " + String(id);
  Serial.println(templates.c_str());

  uint8_t result = enrollFingerprint(id);

  if (result == FINGERPRINT_OK) {
    String templates = String(id) + F(" stored with success!");
    Serial.println(templates.c_str());
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
      Serial.println(F("Image too messy"));
      break;
    case FINGERPRINT_FEATUREFAIL:
      Serial.println(F("Features fail"));
      break;
    case FINGERPRINT_INVALIDIMAGE:
      Serial.println(F("Invalid image"));
      break;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println(F("Communication error"));
      break;
    case FINGERPRINT_BADLOCATION:
      Serial.println(F("Bad location"));
      break;
    case FINGERPRINT_IMAGEFAIL:
      Serial.println(F("Imaging error"));
      break;
    case FINGERPRINT_FLASHERR:
      Serial.println(F("Error writing to flash"));
      break;
    case FINGERPRINT_ENROLLMISMATCH:
      Serial.println(F("Finger did not match"));
      break;
    case FINGERPRINT_NOFINGER:
      Serial.println(F("No finger"));
      break;
    default:
      Serial.println(F("Unknown error"));
      break;
  }
}
