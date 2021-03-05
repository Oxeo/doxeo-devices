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
#include <Adafruit_Fingerprint.h>

#define FINGER_RX 4
#define FINGER_TX 5
#define FINGER_WAKEUP 3
#define FINGER_POWER 6

SoftwareSerial mySerial(FINGER_TX, FINGER_RX);
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&mySerial);
MyMessage msg(0, V_CUSTOM);

unsigned long _timer = 0;

void before()
{
  pinMode(FINGER_WAKEUP, INPUT);
  startFingerPrint();
}

void setup() {
  finger.begin(57600);
  delay(5);

  if (finger.verifyPassword()) {
    send(msg.set(F("fingerprint ok")));

    Serial.println(F("Reading sensor parameters"));
    finger.getParameters();
    Serial.print(F("Status: 0x")); Serial.println(finger.status_reg, HEX);
    Serial.print(F("Sys ID: 0x")); Serial.println(finger.system_id, HEX);
    Serial.print(F("Capacity: ")); Serial.println(finger.capacity);
    Serial.print(F("Security level: ")); Serial.println(finger.security_level);
    Serial.print(F("Device address: ")); Serial.println(finger.device_addr, HEX);
    Serial.print(F("Packet len: ")); Serial.println(finger.packet_len);
    Serial.print(F("Baud rate: ")); Serial.println(finger.baud_rate);

    finger.getTemplateCount();

    if (finger.templateCount == 0) {
      Serial.print("Sensor doesn't contain any fingerprint data. Please run the 'enroll' example.");
    }
    else {
      Serial.println("Waiting for valid finger...");
      Serial.print("Sensor contains "); Serial.print(finger.templateCount); Serial.println(" templates");
    }
  } else {
    send(msg.set(F("fingerprint not found")));
  }
}

void presentation() {
  sendSketchInfo("FingerPrint", "1.0");
  present(0, S_CUSTOM);
}

void receive(const MyMessage &message)
{
  if (message.type == V_CUSTOM && message.sensor == 0) {
    Serial.println(message.getString());
  }
}

void loop() {
  if (digitalRead(FINGER_WAKEUP) == HIGH && millis() - _timer > 2000UL) {
    stopFingerPrint();
    send(msg.set(F("sleep")));
    sleep(digitalPinToInterrupt(FINGER_WAKEUP), LOW, 0);
    startFingerPrint();
    _timer = millis();
  } else {
    uint8_t result = getFingerprintID();

    if (result != FINGERPRINT_NOFINGER) {
      Serial.println(result);
    }
    
    delay(50);

    if (digitalRead(FINGER_WAKEUP) == LOW) {
      _timer = millis();
    }
  }
}

uint8_t getFingerprintID() {
  uint8_t p = finger.getImage();
  switch (p) {
    case FINGERPRINT_OK:
      Serial.println("Image taken");
      break;
    case FINGERPRINT_NOFINGER:
      //Serial.println("No finger detected");
      return p;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("Communication error");
      return p;
    case FINGERPRINT_IMAGEFAIL:
      Serial.println("Imaging error");
      return p;
    default:
      Serial.println("Unknown error");
      return p;
  }

  // OK success!

  p = finger.image2Tz();
  switch (p) {
    case FINGERPRINT_OK:
      Serial.println("Image converted");
      break;
    case FINGERPRINT_IMAGEMESS:
      Serial.println("Image too messy");
      return p;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("Communication error");
      return p;
    case FINGERPRINT_FEATUREFAIL:
      Serial.println("Could not find fingerprint features");
      return p;
    case FINGERPRINT_INVALIDIMAGE:
      Serial.println("Could not find fingerprint features");
      return p;
    default:
      Serial.println("Unknown error");
      return p;
  }

  // OK converted!
  p = finger.fingerSearch();
  if (p == FINGERPRINT_OK) {
    Serial.println("Found a print match!");
    finger.LEDcontrol(FINGERPRINT_LED_ON, 0, FINGERPRINT_LED_BLUE);
    delay(1000);
    finger.LEDcontrol(FINGERPRINT_LED_OFF, 0, FINGERPRINT_LED_BLUE);
  } else if (p == FINGERPRINT_PACKETRECIEVEERR) {
    Serial.println("Communication error");
    return p;
  } else if (p == FINGERPRINT_NOTFOUND) {
    finger.LEDcontrol(FINGERPRINT_LED_FLASHING, 25, FINGERPRINT_LED_RED, 10);
    Serial.println("Did not find a match");
    send(msg.set(F("ko")));
    return p;
  } else {
    Serial.println("Unknown error");
    return p;
  }

  // found a match!
  Serial.print("Found ID #"); Serial.print(finger.fingerID);
  Serial.print(" with confidence of "); Serial.println(finger.confidence);

  send(msg.set(F("ok")));
  return finger.fingerID;
}

void startFingerPrint() {
  pinMode(FINGER_POWER, OUTPUT);
  digitalWrite(FINGER_POWER, LOW);
  pinMode(FINGER_RX, OUTPUT);
  mySerial.println("a"); // fix first send
  delay(30);
}

void stopFingerPrint() {
  pinMode(FINGER_RX, INPUT);  // because mySerial.end() is not working
  pinMode(FINGER_POWER, INPUT);
}
