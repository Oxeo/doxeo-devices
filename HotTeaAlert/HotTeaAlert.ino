#define MY_DEBUG
#define MY_RADIO_RF24
#define MY_RF24_PA_LEVEL (RF24_PA_MAX)

#include <MySensors.h>
#include <Vcc.h>
#include <Wire.h>  // A4 => SDA, A5 => SCL

#define I2C_ADDR 0x5A // Adresse par défaut du capteur IR
#define STATE_ID 0
#define TEMP_ID 1
#define TEMP_POWER A0
#define SWITCH_PIN 2
#define RED_LED_PIN 3
#define GREEN_LED_PIN 4
#define BLUE_LED_PIN 5
#define EEPROM_TARGET_TEMP 0
#define EEPROM_MODE 1

MyMessage stateMsg(STATE_ID, V_TEXT);
MyMessage temperatureMsg(TEMP_ID, V_TEMP);

enum State_enum {SLEEPING, STARTING, HOT, READY, COLD};
const char *STATE_STR[] = {"Sleeping", "Starting", "Hot", "Ready", "Cold"};

int _targetTemperature = -100;
int _oldTemperature = -100;
uint8_t _state = SLEEPING;
unsigned long _coldCpt = 0;
int _mode = 1;

static uint8_t _oldBatteryPcnt = 200;  // Initialize to 200 to assure first time value will be sent.
const float _vccMin        = 2.8;      // Minimum expected Vcc level, in Volts: Brownout at 2.8V    -> 0%
const float _vccMax        = 2.0 * 1.6; // Maximum expected Vcc level, in Volts: 2xAA fresh Alkaline -> 100%
const float _vccCorrection = 1.0;      // Measured Vcc by multimeter divided by reported Vcc
static Vcc _vcc(_vccCorrection);

void before()
{
  pinMode(BLUE_LED_PIN, OUTPUT);
  digitalWrite(BLUE_LED_PIN, HIGH);
  wait(1000);
  digitalWrite(BLUE_LED_PIN, LOW);
}

void presentation()
{
  sendSketchInfo("Hot Tea Alert", "1.0");
  present(STATE_ID, S_INFO, "State status");
  present(TEMP_ID, S_TEMP, "Tea temperature");
}

void setup()
{
  pinMode(SWITCH_PIN, INPUT);
  pinMode(RED_LED_PIN, OUTPUT);
  pinMode(GREEN_LED_PIN, OUTPUT);
  pinMode(BLUE_LED_PIN, OUTPUT);
  loadTargetTemperature();
  loadMode();
  Wire.begin(); // Init I2C Bus
  runningLight();
  changeState(SLEEPING);
}

void loop()
{
  int temperature = round(getTemperature());

  #ifdef MY_DEBUG
    Serial.print(F("Temperature: "));
    Serial.println(temperature);
  #endif

  if (_state == SLEEPING) {
    if (temperature < 40) {
      if (sleep(0, FALLING, 30000) == 0) {
        delay(3000);
        if (digitalRead(SWITCH_PIN) == LOW) {
          if (_mode == 1) {
            changeMode(2);
          } else {
            changeMode(1);
          }

          pinMode(BLUE_LED_PIN, OUTPUT);
          blinkBlueLed(_mode);
          pinMode(BLUE_LED_PIN, INPUT);
        }
      }
    } else {
      changeState(STARTING);
    }
  } else {
    if (_oldTemperature != temperature) {
      _oldTemperature = temperature;

      if (_mode == 2) {
        send(temperatureMsg.set(temperature));
      }

      if (temperature > _targetTemperature && _state != HOT) {
        changeState(HOT);
      } else if (temperature <= _targetTemperature
                 && temperature >= _targetTemperature - 10
                 && _state != READY) {
        changeState(READY);
      } else if (temperature < _targetTemperature - 10 && _state != COLD) {
        changeState(COLD);
      }
    }

    if (_state == COLD) {
      _coldCpt++;
      
      if (_coldCpt >= 600) {  // sleeping mode after 10 minutes
        changeState(SLEEPING);
      }
    }

    if (sleep(0, FALLING, 1000) == 0) {
      delay(1000);
      if (digitalRead(SWITCH_PIN) == LOW) {
        saveTargetTemperature(temperature);
        blinkRedLed();
        changeState(STARTING);
      }
    }
  }
}

void changeState(uint8_t state) {
  _state = state;
  
  #ifdef MY_DEBUG
      Serial.print(F("State: "));
      Serial.println(STATE_STR[state]);
  #endif

  send(stateMsg.set(STATE_STR[state]));
  
  switch (_state) {
    case SLEEPING:
      digitalWrite(RED_LED_PIN, LOW);
      digitalWrite(GREEN_LED_PIN, LOW);
      digitalWrite(BLUE_LED_PIN, LOW);
      pinMode(RED_LED_PIN, INPUT);
      pinMode(GREEN_LED_PIN, INPUT);
      pinMode(BLUE_LED_PIN, INPUT);
      reportBatteryLevel();
      break;
    case STARTING:
      pinMode(RED_LED_PIN, OUTPUT);
      pinMode(GREEN_LED_PIN, OUTPUT);
      pinMode(BLUE_LED_PIN, OUTPUT);
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
      break;
    case COLD:
      digitalWrite(RED_LED_PIN, LOW);
      digitalWrite(GREEN_LED_PIN, LOW);
      digitalWrite(BLUE_LED_PIN, HIGH);
      _coldCpt = 0;
      break;
  }
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

  #ifdef MY_DEBUG
    Serial.print(F("New target temperature saved: "));
    Serial.println(_targetTemperature);
  #endif
}

void loadTargetTemperature() {
  _targetTemperature = loadState(EEPROM_TARGET_TEMP);

  if (_targetTemperature < 30 || _targetTemperature > 200) {
    _targetTemperature = 70;
  }

  #ifdef MY_DEBUG
    Serial.print(F("Target Temperature in EEPROM: "));
    Serial.println(_targetTemperature);
  #endif
}

void loadMode() {
  _mode = loadState(EEPROM_MODE);

  if (_mode < 1 || _mode > 2) {
    _mode = 1;
  }

  #ifdef MY_DEBUG
    Serial.print(F("Mode in EEPROM: "));
    Serial.println(_mode);
  #endif
}

void changeMode(int mode) {
  if (mode == _mode) {
    return;
  }

  saveState(EEPROM_MODE, mode);
  _mode = mode;

  #ifdef MY_DEBUG
    Serial.print(F("New mode saved: "));
    Serial.println(_mode);
  #endif
}

float getTemperature() {
  pinMode(TEMP_POWER, OUTPUT);
  digitalWrite(TEMP_POWER, HIGH);
  sleep(30);

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
  Wire.endTransmission();

  // Calcul de la température
  const float tempFactor = 0.02; // 0.02°C par LSB -> résolution du MLX90614
  float tempData = (tempFactor * data) - 0.01;
  float celsius = tempData - 273.15; // Conversion des degrés Kelvin en degrés Celsius

  // set power pin for DS18B20 to input before sleeping, saves power
  digitalWrite(TEMP_POWER, LOW);
  pinMode(TEMP_POWER, INPUT);

  return celsius;
}

void reportBatteryLevel() {
  const uint8_t batteryPcnt = static_cast<uint8_t>(0.5 + _vcc.Read_Perc(_vccMin, _vccMax));

#ifdef MY_DEBUG
  Serial.print(F("Vbat "));
  Serial.print(_vcc.Read_Volts());
  Serial.print(F("\tPerc "));
  Serial.println(batteryPcnt);
#endif

  // Battery readout should only go down. So report only when new value is smaller than previous one.
  if ( batteryPcnt < _oldBatteryPcnt )
  {
    sendBatteryLevel(batteryPcnt);
    _oldBatteryPcnt = batteryPcnt;
  }
}

void runningLight() {
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

void blinkBlueLed(char numberOfBlink) {
  digitalWrite(RED_LED_PIN, LOW);
  digitalWrite(GREEN_LED_PIN, LOW);
  digitalWrite(BLUE_LED_PIN, LOW);

  for (char i = 0; i < numberOfBlink * 2; i++) {
    if (i % 2 == 0) {
      digitalWrite(BLUE_LED_PIN, HIGH);
    } else {
      digitalWrite(BLUE_LED_PIN, LOW);
    }

    delay(1000);
  }

  digitalWrite(BLUE_LED_PIN, LOW);
}

