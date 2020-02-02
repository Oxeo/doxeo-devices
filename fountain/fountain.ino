// Enable debug prints to serial monitor
//#define MY_DEBUG

// Enable and select radio type attached
#define MY_RADIO_RF24

// Enable debug prints related to the RF24 driver.
//#define MY_DEBUG_VERBOSE_RF24

// Enable IRQ pin
#define MY_RX_MESSAGE_BUFFER_FEATURE
#define MY_RF24_IRQ_PIN (2)

// RF24 PA level
#define MY_RF24_PA_LEVEL (RF24_PA_MAX)

// Enable repeater functionality
//#define MY_REPEATER_FEATURE

#include <MySensors.h>
#include <SPI.h>
#include <OneWire.h>
#include <DallasTemperature.h>

#define PIN_WATER_SENSOR1 A1
#define PIN_WATER_PUMP1 7
#define PIN_WATER_SENSOR2 3
#define PIN_WATER_PUMP2 6
#define PIN_LIGHT 8
#define PIN_ALIM_TEMPERATURE 4
#define PIN_TEMPERATURE 5

#define TEMPERATURE_PRECISION 11 //Max 12 bit, min 9 bit

#define TEMPERATURE_ID 0
#define PUMP1_ID 1
#define PUMP2_ID 2
#define LIGHT_ID 3

// Timer
unsigned long pump1Timer = 0;
unsigned long pump2Timer = 0;
unsigned long lightTimer = 0;
unsigned long _heartbeatTime = 0;

// Temperature sensor
OneWire oneWire(PIN_TEMPERATURE);
DallasTemperature sensors(&oneWire);
byte dallasSensorAddress[8];

// Status
bool lightOn;
bool pump1On;
bool pump2On;
bool pump1IsRunning;
bool pump2IsRunning;
unsigned long lastSendTemperatureTime = 0;

// mode on off auto for pump1
unsigned long pump1AutoModeTimeOn = 0;
unsigned long pump1AutoModeTimeOff = 0;
unsigned long lastChangedWaterPump1Status = 0;

MyMessage msgTemperature(TEMPERATURE_ID, V_TEMP);
MyMessage msgPump1(PUMP1_ID, V_CUSTOM);
MyMessage msgPump2(PUMP2_ID, V_CUSTOM);
MyMessage msgLight(LIGHT_ID, V_CUSTOM);

void before()
{
  // init PIN
  pinMode(PIN_LIGHT, OUTPUT);
  pinMode(PIN_WATER_PUMP1, OUTPUT);
  pinMode(PIN_WATER_SENSOR1, INPUT_PULLUP);
  pinMode(PIN_WATER_PUMP2, OUTPUT);
  pinMode(PIN_WATER_SENSOR2, INPUT_PULLUP);

  digitalWrite(PIN_WATER_PUMP1, LOW);
  digitalWrite(PIN_WATER_PUMP2, LOW);
}

void setup() {
  initializeDallasSensor();

  enablePump1(0);
  enablePump2(0);
  enableLight(5); // 5 seconds
}

void presentation() {
  // Send the sketch version information to the gateway and Controller
  sendSketchInfo("Fountain management", "1.1");

  // Present sensor to controller
  present(TEMPERATURE_ID, S_TEMP, "temperature");
  present(PUMP1_ID, S_CUSTOM, "pump 1");
  present(PUMP2_ID, S_CUSTOM, "pump 2");
  present(LIGHT_ID, S_CUSTOM, "light");
}

void receive(const MyMessage &message)
{
  if (message.type == V_CUSTOM) {
    String data = message.getString();

    if (message.sensor == PUMP1_ID) {
      int timeSelectedMinute = parseMsg(data, '-', 0).toInt();
      int timeOnMinute = parseMsg(data, '-', 1).toInt();
      int timeOffMinute = parseMsg(data, '-', 2).toInt();

      enablePump1(timeSelectedMinute * 60, timeOnMinute * 60, timeOffMinute * 60);
    }

    if (message.sensor == PUMP2_ID) {
      int timeSelectedMinute = data.toInt();
      enablePump2(timeSelectedMinute * 60);
    }

    if (message.sensor == LIGHT_ID) {
      int timeSelectedMinute = data.toInt();
      enableLight(timeSelectedMinute * 60);
    }
  }
}

void loop() {
  if ((millis() - lastSendTemperatureTime) >= 600000) {
    lastSendTemperatureTime = millis();

    float temp = getDallasTemperature();
    send(msgTemperature.set(temp, 1));
  }

  if (lightOn || pump1On || pump2On) {

    if (lightOn && lightTimer < millis()) {
      enableLight(0);
    }

    if (pump1On && pump1Timer < millis()) {
      enablePump1(0);
    }

    if (pump2On && pump2Timer < millis()) {
      enablePump2(0);
    }

    if (pump1On) {
      // manage water sensor
      if (digitalRead(PIN_WATER_SENSOR1) == HIGH) {
        enablePump1(0);
        send(msgPump1.set("no water"));
      }

      // manage on off auto mode
      if (pump1AutoModeTimeOn != 0 && pump1AutoModeTimeOff != 0) {
        if (pump1IsRunning && millis() > lastChangedWaterPump1Status + pump1AutoModeTimeOn) {
          startPump1(false);
          send(msgPump1.set("stand by"));
        } else if (pump1IsRunning == false && millis() > lastChangedWaterPump1Status + pump1AutoModeTimeOff) {
          startPump1(true);
          send(msgPump1.set("started"));
        }
      }
    }

    if (pump2On) {
      // manage water sensor
      if (digitalRead(PIN_WATER_SENSOR2) == HIGH) {
        enablePump2(0);
        send(msgPump2.set("no water"));
      }
    }
  }

  if (millis() - _heartbeatTime >= 60000) {
    sendHeartbeat();
    _heartbeatTime = millis();
  }
}

void startPump1(bool start) {
  if (start) {
    pump1IsRunning = true;
    digitalWrite(PIN_WATER_PUMP1, HIGH);
  } else {
    pump1IsRunning = false;
    digitalWrite(PIN_WATER_PUMP1, LOW);
  }
  lastChangedWaterPump1Status = millis();
}

void startPump2(bool start) {
  if (start) {
    pump2IsRunning = true;
    digitalWrite(PIN_WATER_PUMP2, HIGH);
  } else {
    pump2IsRunning = false;
    digitalWrite(PIN_WATER_PUMP2, LOW);
  }
}

void enablePump1(unsigned long durationSecond) {
  enablePump1(durationSecond, 0, 0);
}

void enablePump1(unsigned long durationSecond, unsigned long timeOnSecond, unsigned long timeOffSecond) {
  if (durationSecond > 0) {
    pinMode(PIN_WATER_SENSOR1, INPUT_PULLUP);
    startPump1(true);
    pump1Timer = millis() + durationSecond * 1000;
    pump1AutoModeTimeOn = timeOnSecond * 1000;
    pump1AutoModeTimeOff = timeOffSecond * 1000;
    pump1On = true;
    send(msgPump1.set("started"));
  } else {
    startPump1(false);
    pinMode(PIN_WATER_SENSOR1, INPUT); // to save energy
    pump1Timer = 0;
    pump1On = false;
    send(msgPump1.set("stopped"));
  }
}

void enablePump2(unsigned long durationSecond) {
  if (durationSecond > 0) {
    pinMode(PIN_WATER_SENSOR2, INPUT_PULLUP);
    startPump2(true);
    pump2Timer = millis() + durationSecond * 1000;
    pump2On = true;
    send(msgPump2.set("started"));
  } else {
    startPump2(false);
    pinMode(PIN_WATER_SENSOR2, INPUT); // to save energy
    pump2Timer = 0;
    pump2On = false;
    send(msgPump2.set("stopped"));
  }
}

void enableLight(unsigned long durationSecond) {
  if (durationSecond > 0) {
    digitalWrite(PIN_LIGHT, HIGH);
    lightTimer = millis() + durationSecond * 1000;
    lightOn = true;
    send(msgLight.set("started"));
  } else {
    digitalWrite(PIN_LIGHT, LOW);
    lightTimer = 0;
    lightOn = false;
    send(msgLight.set("stopped"));
  }
}

void initializeDallasSensor() {
  pinMode(PIN_ALIM_TEMPERATURE, OUTPUT);
  digitalWrite(PIN_ALIM_TEMPERATURE, HIGH);

  // Allow 50ms for the sensor to be ready
  delay(50);

  sensors.begin();
  sensors.setWaitForConversion(false);
  int numSensors = sensors.getDeviceCount();
  oneWire.search(dallasSensorAddress);
  sensors.setResolution(dallasSensorAddress, TEMPERATURE_PRECISION);

  // set power pin for DS18B20 to input before sleeping, saves power
  digitalWrite(PIN_ALIM_TEMPERATURE, LOW);
  pinMode(PIN_ALIM_TEMPERATURE, INPUT);
}

float getDallasTemperature() {
  pinMode(PIN_ALIM_TEMPERATURE, OUTPUT);
  digitalWrite(PIN_ALIM_TEMPERATURE, HIGH);
  sleep(30);

  sensors.setResolution(dallasSensorAddress, TEMPERATURE_PRECISION);
  sensors.requestTemperatures(); // Send the command to get temperatures

  // 9bit requres 95ms, 10bit 187ms, 11bit 375ms and 12bit resolution takes 750ms
  sleep(400);

  // Fetch temperature
  float temperature = sensors.getTempC(dallasSensorAddress);

  // set power pin for DS18B20 to input before sleeping, saves power
  digitalWrite(PIN_ALIM_TEMPERATURE, LOW);
  pinMode(PIN_ALIM_TEMPERATURE, INPUT);

  return temperature;
}

String parseMsg(String data, char separator, int index)
{
  int found = 0;
  int strIndex[] = {0, -1};
  int maxIndex = data.length() - 1;

  for (int i = 0; i <= maxIndex && found <= index; i++) {
    if (data.charAt(i) == separator || i == maxIndex) {
      found++;
      strIndex[0] = strIndex[1] + 1;
      strIndex[1] = (i == maxIndex) ? i + 1 : i;
    }
  }

  return found > index ? data.substring(strIndex[0], strIndex[1]) : "";
}

void wakeUp()
{
  // do nothing
}
