#define MY_CORE_ONLY
//#define DEBUG

#include <MySensors.h>
#include <Wire.h>  // A4 => SDA, A5 => SCL

#define I2C_ADDR 0x5A // Adresse par défaut du capteur IR
#define TEMP_POWER A0
#define SWITCH_PIN 2
#define BUZZER_PIN 4
#define RED_LED_PIN 6
#define GREEN_LED_PIN 7 
#define BLUE_LED_PIN 8
#define EEPROM_TARGET_TEMP 0

enum State_enum {SLEEPING, STARTING, HOT, READY, COLD};
const char *STATE_STR[] = {"Sleeping", "Starting", "Hot", "Ready", "Cold"};

int _targetTemperature = -100;
int _oldTemperature = -100;
uint8_t _state = SLEEPING;
unsigned long _coldCpt = 0;
unsigned long _readyCpt = 0;
int _blinkCpt = 0;
bool _blueLedOn = false;

void setup()
{
  pinMode(SWITCH_PIN, INPUT);
  runningLight();
  loadTargetTemperature();
  changeState(SLEEPING);
}

void loop()
{
  int temperature = round(getTemperature());
  
  #ifdef DEBUG
    Serial.print(F("Temperature: "));
    Serial.println(temperature);
  #endif

  if (_state == SLEEPING) {
    if (temperature > 40) {
      changeState(STARTING);
    } else {
      sleep(60000);
    }
  } else {
    if (_oldTemperature != temperature) {
      _oldTemperature = temperature;

      if (temperature > _targetTemperature && _state != HOT) {
        changeState(HOT);
      } else if (temperature <= _targetTemperature
                 && temperature >= _targetTemperature - 5
                 && _state != READY) {
        changeState(READY);
      } else if (temperature < _targetTemperature - 5 && _state != COLD) {
        changeState(COLD);
      }
    }

    if (_state == READY) {
      if (_readyCpt > 0) {
        _readyCpt--;
        if (_readyCpt == 0) {
          digitalWrite(BUZZER_PIN, HIGH);
          sleep(2000);
          digitalWrite(BUZZER_PIN, LOW);
        }
      }
    } else if (_state == COLD) {
      if (temperature < 40 && _blueLedOn) {
        digitalWrite(BLUE_LED_PIN, LOW);
        _blueLedOn = false;
      } else if (temperature > 40 && !_blueLedOn) {
        digitalWrite(BLUE_LED_PIN, HIGH);
        _blueLedOn = true;
      }
      
      _coldCpt++;
      if (_coldCpt >= 60) {  // sleeping mode after 1 minutes
        changeState(SLEEPING);
      }
    } else if (_state == HOT) {
      if (_blinkCpt % 3 == 0) {
        digitalWrite(RED_LED_PIN, HIGH);
        delay(500);
        digitalWrite(RED_LED_PIN, LOW);
      }
      _blinkCpt++;
    }

    sleep(1000);
    if (digitalRead(SWITCH_PIN) == LOW) {
      saveTargetTemperature(temperature);
      blinkRedLed();
      changeState(STARTING);
    }
  }
}

void changeState(uint8_t state) {
  #ifdef DEBUG
      Serial.print(F("State: "));
      Serial.println(STATE_STR[state]);
  #endif
  
  switch (state) {
    case SLEEPING:
      digitalWrite(RED_LED_PIN, LOW);
      digitalWrite(GREEN_LED_PIN, LOW);
      digitalWrite(BLUE_LED_PIN, LOW);
      pinMode(RED_LED_PIN, INPUT);
      pinMode(GREEN_LED_PIN, INPUT);
      pinMode(BLUE_LED_PIN, INPUT);
      pinMode(BUZZER_PIN, INPUT);
      break;
    case STARTING:
      pinMode(RED_LED_PIN, OUTPUT);
      pinMode(GREEN_LED_PIN, OUTPUT);
      pinMode(BLUE_LED_PIN, OUTPUT);
      pinMode(BUZZER_PIN, OUTPUT);
      _oldTemperature = -100;
      break;
    case HOT:
      digitalWrite(RED_LED_PIN, HIGH);
      digitalWrite(GREEN_LED_PIN, LOW);
      digitalWrite(BLUE_LED_PIN, LOW);
      break;
    case READY:
      digitalWrite(RED_LED_PIN, LOW);
      digitalWrite(GREEN_LED_PIN, HIGH);
      digitalWrite(BLUE_LED_PIN, LOW);
      if (_state == HOT) {
        _readyCpt = 5;          
      } else {
        _readyCpt = 0;
      }
      break;
    case COLD:
      digitalWrite(RED_LED_PIN, LOW);
      digitalWrite(GREEN_LED_PIN, LOW);
      digitalWrite(BLUE_LED_PIN, HIGH);
      _blueLedOn = true;
      _coldCpt = 0;
      break;
  }

  _state = state;
}

void saveTargetTemperature(int temperature) {
  if (temperature == _targetTemperature) {
    return;
  }

  if (temperature < 30 || temperature > 200) {
    return;
  }

  saveState(EEPROM_TARGET_TEMP, temperature);
  _targetTemperature = temperature;

  #ifdef DEBUG
    Serial.print(F("New target temperature saved: "));
    Serial.println(_targetTemperature);
  #endif
}

void loadTargetTemperature() {
  _targetTemperature = loadState(EEPROM_TARGET_TEMP);

  if (_targetTemperature < 30 || _targetTemperature > 200) {
    _targetTemperature = 60;
  }

  #ifdef DEBUG
    Serial.print(F("Target Temperature in EEPROM: "));
    Serial.println(_targetTemperature);
  #endif
}

float getTemperature() {
  pinMode(TEMP_POWER, OUTPUT);
  digitalWrite(TEMP_POWER, HIGH);
  sleep(50);
  Wire.begin(); // Init I2C Bus
  
  // Données brute de température
  uint16_t data;

  // Commande de lecture de la RAM à l'adresse 0x07
  Wire.beginTransmission(I2C_ADDR);
  Wire.write(0x07);
  Wire.endTransmission(false);

  // Lecture des données : 1 mot sur 16 bits + octet de contrôle (PEC)
  Wire.requestFrom(I2C_ADDR, 3, false);
  while (Wire.available() < 3);
  data = Wire.read();
  data |= (Wire.read() & 0x7F) << 8;  // Le MSB est ignoré (bit de contrôle d'erreur)
  Wire.read(); // PEC
  Wire.endTransmission(true);

  // set power pin to input before sleeping, saves power
  digitalWrite(A4, LOW);
  pinMode(A4, INPUT);
  digitalWrite(A5, LOW);
  pinMode(A5, INPUT);
  digitalWrite(TEMP_POWER, LOW);
  pinMode(TEMP_POWER, INPUT);

  // Calcul de la température
  const float tempFactor = 0.02; // 0.02°C par LSB -> résolution du MLX90614
  float tempData = (tempFactor * data) - 0.01;
  float celsius = tempData - 273.15; // Conversion des degrés Kelvin en degrés Celsius

  return celsius;
}

void runningLight() {
  pinMode(GREEN_LED_PIN, OUTPUT);
  pinMode(BLUE_LED_PIN, OUTPUT);
  pinMode(RED_LED_PIN, OUTPUT);
  
  digitalWrite(RED_LED_PIN, HIGH);
  wait(500);
  digitalWrite(RED_LED_PIN, LOW);
  digitalWrite(GREEN_LED_PIN, HIGH);
  wait(500);
  digitalWrite(GREEN_LED_PIN, LOW);
  digitalWrite(BLUE_LED_PIN, HIGH);
  wait(500);
  digitalWrite(BLUE_LED_PIN, LOW);
}

void blinkRedLed() {
  digitalWrite(RED_LED_PIN, LOW);
  digitalWrite(GREEN_LED_PIN, LOW);
  digitalWrite(BLUE_LED_PIN, LOW);

  for (char i = 0; i < 40; i++) {
    if (i % 2 == 0) {
      digitalWrite(RED_LED_PIN, HIGH);
    } else {
      digitalWrite(RED_LED_PIN, LOW);
    }

    delay(50);
  }

  digitalWrite(RED_LED_PIN, LOW);
}
