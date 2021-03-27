// Enable debug prints to serial monitor
//#define MY_DEBUG

#define MY_RADIO_RF24

// Enable IRQ pin
#define MY_RX_MESSAGE_BUFFER_FEATURE
#define MY_RF24_IRQ_PIN (2)

#include <MySensors.h>
#include <Parser.h>
#include <SoftwareSerial.h>

#define CAM_POWER_PIN 5
#define BATTERY_LEVEL_PIN A0
#define ESP32_RX_PIN 6
#define ESP32_TX_PIN 7
#define ESP32_P13_PIN A1
#define ESP32_P12_PIN A2
#define EEPROM_VOLTAGE_CORRECTION 0

struct FuelGauge {
  float voltage;
  int percent;
};

enum state_enum {SLEEPING, RUNNING, STOP_CAM, GOING_TO_SLEEP};
uint8_t _state;

enum mode_enum {NORMAL_MODE, NIGHT_MODE, LIGHT_MODE};
uint8_t _mode;

Parser parser = Parser(' ');
MyMessage msg(0, V_CUSTOM);
unsigned long _cpt = 0;
unsigned long _runningTime = 0;
unsigned long _stateTimer = 0;
unsigned long _camStartedTimer = 0;
boolean _transportDisable = false;
boolean _modeSend = false;

SoftwareSerial esp32Serial(ESP32_TX_PIN, ESP32_RX_PIN); // RX, TX
char esp32Data[25];
byte esp32DataCpt = 0;

float _voltageCorrection = 1;
const int _lionTab[] = {3500, 3550, 3590, 3610, 3640, 3710, 3790, 3880, 3970, 4080, 4200};
int _batteryPercent = 101;

void before() {
  pinMode(ESP32_P13_PIN, INPUT);
  pinMode(CAM_POWER_PIN, OUTPUT);
  digitalWrite(CAM_POWER_PIN, LOW);

  //saveVoltageCorrection(0.988077859); // Measured by multimeter divided by reported
  _voltageCorrection = getVoltageCorrection();
  analogReference(INTERNAL);
  getFuelGauge(); // first read is wrong
  FuelGauge gauge = getFuelGauge();

  if (gauge.voltage <= 3.5) {
    sleep(0);
  }
}

void setup() {
  _state = SLEEPING;
  _cpt = 0;
  _batteryPercent = 101;
  reportBatteryLevel();
}

void presentation() {
  sendSketchInfo("Cam Actuator Lithium", "1.1");
  present(0, S_CUSTOM);
}

void receive(const MyMessage &message)
{
  if (message.type == V_CUSTOM && message.sensor == 0) {
    parser.parse(message.getString());

    if (parser.isEqual(0, "start") && parser.get(1) != NULL) {
      _runningTime = parser.getInt(1);

      if (parser.isEqual(2, "n")) {
        _mode = NIGHT_MODE;
      } else if (parser.isEqual(2, "l")) {
        _mode = LIGHT_MODE;
      } else {
        _mode = NORMAL_MODE;
      }

      if (_state == RUNNING) {
        _stateTimer = millis();
      } else {
        changeState(RUNNING);
      }
    } else if (parser.isEqual(0, "stop")) {
      changeState(STOP_CAM);
    } else if (parser.isEqual(0, "ssid") && parser.get(1) != NULL) {
      if (_state == RUNNING) {
        esp32Serial.print("ssid:");
        esp32Serial.println(parser.get(1));
      }
    }
  }
}

void loop() {
  if (_state == SLEEPING) {
    sleep(985);
    RF24_startListening();
    wait(15);
    _cpt += 1;

    if (_cpt == 21600) { // 6H
      _cpt = 0;
      reportBatteryLevel();
      sendHeartbeat();
    }
  } else if (_state == RUNNING) {
    while (esp32Serial.available()) {
      char character = esp32Serial.read();
      esp32Data[esp32DataCpt++] = character;

      if (character == '\0' || character == '\n' || esp32DataCpt == 24) {
        esp32Data[esp32DataCpt] = '\0';

        /*if (esp32DataCpt > 3 && esp32Data[0] == 'r' && esp32Data[1] == 'e' && esp32Data[2] == 'a' && esp32Data[3] == 'd' && esp32Data[4] == 'y') {
          if (_mode == NIGHT_MODE) {
            esp32Serial.println("gc6");
          } else if (_mode == LIGHT_MODE) {
            esp32Serial.println("fs8");
          }
        }*/

        if (strstr(esp32DataCpt, "send 1" ) != NULL || _modeSend == false) {
          if (_mode == NIGHT_MODE) {
            esp32Serial.println("gc6");
          } else if (_mode == LIGHT_MODE) {
            esp32Serial.println("fs8");
          }
          
          _modeSend = true;
        }

        if (_transportDisable) {
          transportReInitialise();
          _transportDisable = false;
        }
        send(msg.set(esp32Data));
        esp32DataCpt = 0;
      }
    }

    if (millis() - _stateTimer > _runningTime * 1000UL) {
      changeState(STOP_CAM);
    }
  } else if (_state == STOP_CAM) {
    if (digitalRead(ESP32_P13_PIN) == LOW || millis() - _stateTimer > 10000) {
      stopCam();
      if (_transportDisable) {
        transportReInitialise();
        _transportDisable = false;
      }
      send(msg.set(F("stopped")));
      reportBatteryLevel();
      changeState(GOING_TO_SLEEP);
    }
  } else if (_state == GOING_TO_SLEEP) {
    if (millis() - _stateTimer > 500) {
      changeState(SLEEPING);
    }
  }
}

void changeState(state_enum state) {
  switch (state) {
    case SLEEPING:
      break;
    case RUNNING:
      startCam();
      _camStartedTimer = millis();
      delay(50); // time to send NRF ACK before sleep
      transportDisable();
      _transportDisable = true;
      //delay(150);
      //sleep(3000);
      //RF24_startListening();
      esp32Serial.begin(9600);
      _modeSend = false;
      break;
    case STOP_CAM:
      esp32Serial.end();
      break;
    case GOING_TO_SLEEP:
      break;
  }

  _state = state;
  _stateTimer = millis();
}

void startCam() {
  digitalWrite(CAM_POWER_PIN, HIGH);
}

void stopCam() {
  digitalWrite(CAM_POWER_PIN, LOW);
}

void reportBatteryLevel() {
  FuelGauge gauge = getFuelGauge();

#ifdef MY_DEBUG
  Serial.print(F("Voltage: "));
  Serial.print(gauge.voltage);
  Serial.print(F(" ("));
  Serial.print(gauge.percent);
  Serial.println(F("%)"));
#endif

  if (gauge.percent < _batteryPercent) {
    String voltageMsg = "voltage-" + String(gauge.voltage) + "-" + String(gauge.percent);
    send(msg.set(voltageMsg.c_str()));
    sendBatteryLevel(gauge.percent);
    _batteryPercent = gauge.percent;
  }

  if (gauge.voltage <= 3.5) {
    stopCam();
    esp32Serial.end();
    send(msg.set(F("Battery too low: sleep")));
    sleep(0);
  }
}

FuelGauge getFuelGauge() {
  FuelGauge gauge;
  int analog = analogRead(BATTERY_LEVEL_PIN);  // 0 - 1023
  float u2 = (analog * 1.1) / 1023.0;
  gauge.voltage = (u2 * (470000.0 + 100000.0)) / 100000.0;
  gauge.voltage *= _voltageCorrection;

  int um = round(gauge.voltage * 1000.0);

  for (byte i = 10; i >= 0; i--) {
    if (um >= _lionTab[i]) {
      if (i == 10) {
        gauge.percent = 100;
      } else {
        gauge.percent = map(um, _lionTab[i], _lionTab[i + 1], i * 10, i * 10 + 10);
      }

      break;
    } else {
      gauge.percent = 0;
    }
  }

  return gauge;
}

void saveVoltageCorrection(float value) {
  byte *b = (byte *)&value;

  for (byte i = 0; i < sizeof(value); i++) {
    saveState(EEPROM_VOLTAGE_CORRECTION + i, b[i]);
  }
}

float getVoltageCorrection() {
  float value = 0.0;
  byte *b = (byte *)&value;

  for (byte i = 0; i < sizeof(value); i++) {
    *b++ = loadState(EEPROM_VOLTAGE_CORRECTION + i);
  }

  return value;
}
