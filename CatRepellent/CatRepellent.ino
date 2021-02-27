//#define MY_DEBUG

#define MY_RADIO_RF24
//#define MY_PARENT_NODE_ID 19
//#define MY_PARENT_NODE_IS_STATIC

// Enable IRQ pin
#define MY_RX_MESSAGE_BUFFER_FEATURE
#define MY_RF24_IRQ_PIN (2)

#include <MySensors.h>
#include <Parser.h>

#define PUMP_PIN 5
#define WATER_SENSOR_PIN A0
#define BATTERY_LEVEL_PIN A1
#define EEPROM_VOLTAGE_CORRECTION 0

struct FuelGauge {
  float voltage;
  int percent;
};

enum state_enum {SLEEPING, RUNNING};
uint8_t _state;

MyMessage msg(0, V_CUSTOM);
Parser parser = Parser(' ');
unsigned long _cpt = 0;
bool _waterTankEmpty;
unsigned long _duration = 10;
unsigned long _startTime = 0;
float _voltageCorrection = 1;
int _batteryPercent = 0;

void before()
{
  pinMode(PUMP_PIN, INPUT);
  pinMode(WATER_SENSOR_PIN, INPUT);
  pinMode(BATTERY_LEVEL_PIN, INPUT);
  _waterTankEmpty = false;
  _state = SLEEPING;
  _cpt = 0;

  //saveVoltageCorrection(0.9845); // Measured by multimeter divided by reported
  _voltageCorrection = getVoltageCorrection();
  analogReference(INTERNAL);
  getFuelGauge(); // first read is wrong
  FuelGauge gauge = getFuelGauge();
}

void setup()
{
  send(msg.set(F("cat repellent started")));
}

void presentation()
{
  sendSketchInfo("Cat Repellent", "2.0");
  present(0, S_CUSTOM);
}

void receive(const MyMessage &message)
{
  if (message.type == V_CUSTOM && message.sensor == 0) {
    parser.parse(message.getString());

    if (parser.isEqual(0, "stop")) {
      stopPump();
    } else if (parser.isEqual(0, "start") && parser.get(1) != NULL) {
      startPump(parser.getInt(1));
    } else {
      send(msg.set(F("invalid command")));
    }
  }
}

void loop()
{
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
    managePump();
  }
}

inline void managePump() {
  if (millis() - _startTime > _duration) {
    stopPump();
  }

  if (millis() - _startTime > 100UL) {
    if (digitalRead(WATER_SENSOR_PIN) == HIGH) {
      stopPump();
      _waterTankEmpty = true;
      send(msg.set(F("water tank is empty")));
    }
  }
}

void changeState(uint8_t state) {
  switch (state) {
    case SLEEPING:
      break;
    case RUNNING:
      _startTime = millis();
      break;
  }

  _state = state;
}

void startPump(int duration) {
  if (_waterTankEmpty && isWaterTankEmpty()) {
    send(msg.set(F("water tank is empty")));
    return;
  }
  
  send(msg.set(F("started")));
  _duration = duration * 1000UL;
  pinMode(PUMP_PIN, OUTPUT);
  digitalWrite(PUMP_PIN, LOW);
  pinMode(WATER_SENSOR_PIN, INPUT_PULLUP);
  changeState(RUNNING);
}

void stopPump() {
  pinMode(PUMP_PIN, INPUT);
  pinMode(WATER_SENSOR_PIN, INPUT);
  send(msg.set(F("stopped")));
  changeState(SLEEPING);
}

bool isWaterTankEmpty() {
  pinMode(WATER_SENSOR_PIN, INPUT_PULLUP);
  wait(15);
  bool result = digitalRead(WATER_SENSOR_PIN) == HIGH;
  pinMode(WATER_SENSOR_PIN, INPUT);
  return result;
}

void reportBatteryLevel() {
  FuelGauge gauge = getFuelGauge();

#ifdef MY_DEBUG
  Serial.print(F("Voltage: "));
  Serial.print(gauge.voltage, 4);
  Serial.print(F(" ("));
  Serial.print(gauge.percent);
  Serial.println(F("%)"));
#endif

  if (gauge.percent != _batteryPercent) {
    String voltageMsg = "voltage-" + String(gauge.voltage) + "-" + String(gauge.percent);
    send(msg.set(voltageMsg.c_str()));
    sendBatteryLevel(gauge.percent);
    _batteryPercent = gauge.percent;
  }
}

FuelGauge getFuelGauge() {
  FuelGauge gauge;
  int analog = analogRead(BATTERY_LEVEL_PIN);  // 0 - 1023
  float u2 = (analog * 1.1) / 1023.0;
  gauge.voltage = (u2 * (1000000.0 + 47000.0)) / 47000.0;
  gauge.voltage *= _voltageCorrection;

  int um = round(gauge.voltage * 1000.0);
  gauge.percent = map(um, 12000, 13000, 0, 100);

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
