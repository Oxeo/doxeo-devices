// Enable debug prints to serial monitor
//#define MY_DEBUG

// Enable REPORT_BATTERY_LEVEL to measure battery level and send changes to gateway
#define REPORT_BATTERY_LEVEL

#define MY_RADIO_RF24
#define MY_RF24_PA_LEVEL (RF24_PA_MAX)
#define MY_TRANSPORT_MAX_TX_FAILURES (3u)

#if defined(MY_DEBUG)
  #define DEBUG_PRINT(str) Serial.println(str);
#else
  #define DEBUG_PRINT(str)
#endif

#include <MySensors.h>
#include <SPI.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// 1 = lounge
// 2 = bedroom
#define SENSOR_CONFIGURATION 0

#if SENSOR_CONFIGURATION == 1
#define PIN_TEMPERATURE 3
#define PIN_ALIM_TEMPERATURE 4
#elif SENSOR_CONFIGURATION == 2
#define PIN_TEMPERATURE 4
#define PIN_ALIM_TEMPERATURE 5
#else
#define PIN_TEMPERATURE A0
#define PIN_ALIM_TEMPERATURE A1
#endif

#define PIN_PHOTOCELL A3
#define PIN_ALIM_PHOTOCELL A5

#ifdef REPORT_BATTERY_LEVEL
#include <Vcc.h>
static uint8_t _oldBatteryPcnt = 200;  // Initialize to 200 to assure first time value will be sent.
const float _vccMin        = 2.8;      // Minimum expected Vcc level, in Volts: Brownout at 2.8V    -> 0%
const float _vccMax        = 2.0*1.6;  // Maximum expected Vcc level, in Volts: 2xAA fresh Alkaline -> 100%
const float _vccCorrection = 1.0;      // Measured Vcc by multimeter divided by reported Vcc
static Vcc _vcc(_vccCorrection); 
#endif

#define TEMPERATURE_PRECISION 11 //Max 12 bit, min 9 bit

// Temperature sensor
OneWire oneWire(PIN_TEMPERATURE);
DallasTemperature sensors(&oneWire);
byte dallasSensorAddress[8];

float temp = 0.0;
int oldLight = -100;
unsigned int _cpt = 0;
bool success = false;

MyMessage msgTemp(0, V_TEMP);
MyMessage msgLight(1, V_LIGHT_LEVEL);

void setup() {
  randomSeed(analogRead(0));//initialise la séquence aléatoir
  initializeDallasSensor();
  pinMode(PIN_PHOTOCELL, INPUT);
}

void presentation() {
  // Send the sketch version information to the gateway and Controller
  sendSketchInfo("Temperature Sensor", "1.2");

  // Present sensor to controller
  present(0, S_TEMP);
  present(1, S_LIGHT_LEVEL);
}

void loop() {
  // Every 10 minutes
  if (_cpt % 60 == 0) {
    temp = getDallasTemperature();

    DEBUG_PRINT(F("Temperature: "));
    DEBUG_PRINT(temp);
    
    send(msgTemp.set(temp, 1));
    reportBatteryLevel();
  }

  int photocell = readPhotocell();

  if (photocell != oldLight) {
    oldLight = photocell;
    DEBUG_PRINT(F("Light: "));
    DEBUG_PRINT(photocell);
    send(msgLight.set(photocell));
  }

  _cpt++;
  sleep(10000);
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

  // Fetch and round temperature to one decimal
  float temperature = sensors.getTempC(dallasSensorAddress);

  // set power pin for DS18B20 to input before sleeping, saves power
  digitalWrite(PIN_ALIM_TEMPERATURE, LOW);
  pinMode(PIN_ALIM_TEMPERATURE, INPUT);

  return temperature;
}

int readPhotocell() {
  pinMode(PIN_ALIM_PHOTOCELL, OUTPUT);
  digitalWrite(PIN_ALIM_PHOTOCELL, HIGH);
  sleep(30);
  
  int value = analogRead(PIN_PHOTOCELL) / 10.23;

  digitalWrite(PIN_ALIM_PHOTOCELL, LOW);
  pinMode(PIN_ALIM_PHOTOCELL, INPUT);

  return value;
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
