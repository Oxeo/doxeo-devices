#define MY_DEBUG

#define MY_RADIO_RF24
#define MY_RF24_PA_LEVEL (RF24_PA_MAX)
#define MY_TRANSPORT_MAX_TX_FAILURES (3u)

#include <MySensors.h>
#include <Parser.h>

#define SENSOR_ID 0
#define PUMP_PIN 5
#define PIR_PIN 3
#define GREEN_LED_PIN A1
#define RED_LED_PIN A3
#define WATER_SENSOR_PIN A0

#define EEPROM_WATER_STATUS 0
#define EEPROM_MODE 1
#define EEPROM_DURATION 2
#define EEPROM_SPACING 3

MyMessage msg(SENSOR_ID, V_CUSTOM);
Parser parser = Parser('-');
bool _risingEvent;
bool _waterTankEmpty;
uint8_t _mode;
uint8_t _duration;
uint8_t _spacing;

void before()
{
  pinMode(PUMP_PIN, OUTPUT);
  pinMode(GREEN_LED_PIN, OUTPUT);
  pinMode(RED_LED_PIN, OUTPUT);

  pinMode(PIR_PIN, INPUT);
  pinMode(WATER_SENSOR_PIN, INPUT);

  digitalWrite(PUMP_PIN, LOW);
  digitalWrite(GREEN_LED_PIN, LOW);
  digitalWrite(RED_LED_PIN, HIGH);
}

void setup()
{
  _mode = readEeprom(EEPROM_MODE, 1);
  _duration = readEeprom(EEPROM_DURATION, 2);
  _spacing = readEeprom(EEPROM_SPACING, 0);
  _waterTankEmpty = loadState(EEPROM_WATER_STATUS);

#ifdef MY_DEBUG
  Serial.println ("Mode: " + String(_mode));
  Serial.println ("Duration: " + String(_duration));
  Serial.println ("Spacing: " + String(_spacing));
#endif

  request(0, V_CUSTOM, 0);

  digitalWrite(RED_LED_PIN, LOW);
  for (byte i = 0; i < 20; i++) {
    if (i % 2 == 0) {
      digitalWrite(GREEN_LED_PIN, HIGH);
    } else {
      digitalWrite(GREEN_LED_PIN, LOW);
    }
    wait(100);
  }

  send(msg.set(F("cat repellent started")));

  sleep(digitalPinToInterrupt(PIR_PIN), RISING, 0);
  _risingEvent = true;
}

void presentation()
{
  sendSketchInfo("Cat Repellent", "1.0");
  present(SENSOR_ID, S_CUSTOM);
}

void receive(const MyMessage &myMsg)
{
  if (myMsg.type == V_CUSTOM && myMsg.sensor == 0) {
#ifdef MY_DEBUG
    Serial.print("New message: ");
    Serial.println(myMsg.getString());
#endif

    parser.parse(myMsg.getString());
    if (parser.get(0) != NULL && parser.get(1) != NULL && parser.get(2) != NULL) {
      _mode = parser.getInt(0);
      _duration = parser.getInt(1);
      _spacing = parser.getInt(2);

#ifdef MY_DEBUG
      Serial.println ("Mode: " + String(_mode));
      Serial.println ("Duration: " + String(_duration));
      Serial.println ("Spacing: " + String(_spacing));
#endif

      // Store mode in eeprom
      if (loadState(EEPROM_MODE) != _mode) {
        saveState(EEPROM_MODE, _mode);
        send(msg.set(F("mode updated")));
      }

      // Store duration in eeprom
      if (loadState(EEPROM_DURATION) != _duration) {
        saveState(EEPROM_DURATION, _duration);
        send(msg.set(F("duration updated")));
      }

      // Store spacing in eeprom
      if (loadState(EEPROM_SPACING) != _spacing) {
        saveState(EEPROM_SPACING, _spacing);
        send(msg.set(F("spacing updated")));
      }
    }
  }
}

void loop()
{
  if (_mode == 3) {
    if (digitalRead(PIR_PIN) == HIGH) {
      digitalWrite(GREEN_LED_PIN, HIGH);
    } else {
      digitalWrite(GREEN_LED_PIN, LOW);
    }
  } else {
    if (digitalRead(PIR_PIN) == HIGH) {
      bool empty = isWaterTankEmpty();

      if (empty) {
        digitalWrite(RED_LED_PIN, HIGH);
      } else {
        digitalWrite(GREEN_LED_PIN, HIGH);
      }

      if ((!empty && _mode == 1) || _mode == 2) {
        digitalWrite(PUMP_PIN, HIGH);
      }

      if (_risingEvent) {
        send(msg.set(F("cat alert")));
      }

      // send water tank alert
      if (!_waterTankEmpty && empty) {
        send(msg.set(F("water tank is empty")));
        _waterTankEmpty = true;
        saveState(EEPROM_WATER_STATUS, _waterTankEmpty);
      } else if (_waterTankEmpty && !empty) {
        send(msg.set(F("water tank is full")));
        _waterTankEmpty = false;
        saveState(EEPROM_WATER_STATUS, _waterTankEmpty);
      }

      if (_risingEvent) {
        request(0, V_CUSTOM, 0);
      }

      wait(_duration * 1000);

      digitalWrite(PUMP_PIN, LOW);
      digitalWrite(GREEN_LED_PIN, LOW);
      digitalWrite(RED_LED_PIN, LOW);

      if (_spacing > 0) {
        sleep(_spacing * 1000);
      }

      _risingEvent = false;
    } else {
      sleep(digitalPinToInterrupt(PIR_PIN), RISING, 0);
      _risingEvent = true;
    }
  }
}

bool isWaterTankEmpty() {
  pinMode(WATER_SENSOR_PIN, INPUT_PULLUP);
  wait(15);
  bool result = digitalRead(WATER_SENSOR_PIN) == HIGH;
  pinMode(WATER_SENSOR_PIN, INPUT);
  return result;
}

uint8_t readEeprom(uint8_t pos, uint8_t defaultValue) {
  uint8_t value = loadState(pos);

  if (value == 0xFF) {
    return defaultValue;
  } else {
    return value;
  }
}
