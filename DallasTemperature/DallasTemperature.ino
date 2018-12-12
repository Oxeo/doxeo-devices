// Enable debug prints to serial monitor
//#define MY_DEBUG

#define MY_RADIO_RF24
#define MY_RF24_PA_LEVEL (RF24_PA_MAX)
#define MY_TRANSPORT_MAX_TX_FAILURES (3u)

#include <MySensors.h>
#include <SPI.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// 1 = lounge
// 2 = bedroom
#define SENSOR_CONFIGURATION 2

#if SENSOR_CONFIGURATION == 1
#define PIN_TEMPERATURE 3
#define PIN_ALIM_TEMPERATURE 4
#elif SENSOR_CONFIGURATION == 2
#define PIN_TEMPERATURE 4
#define PIN_ALIM_TEMPERATURE 5
#endif

#define TEMPERATURE_PRECISION 11 //Max 12 bit, min 9 bit

// Temperature sensor
OneWire oneWire(PIN_TEMPERATURE);
DallasTemperature sensors(&oneWire);
byte dallasSensorAddress[8];

char retryNb = 0;
float temp = 0.0;
bool success = false;

MyMessage msg(0, V_TEMP);

void setup() {
  randomSeed(analogRead(0));//initialise la séquence aléatoir
  initializeDallasSensor();
  retryNb = 0;
}

void presentation() {
  // Send the sketch version information to the gateway and Controller
  sendSketchInfo("Temperature Sensor", "1.2");

  // Present sensor to controller
  present(0, S_TEMP);
}

void loop() {
  temp = getDallasTemperature();
  delay(100);
  success = false;
  
  while (!success) {
    success = send(msg.set(temp, 1));

    if (success) {
      retryNb = 0;
      sleep(600000); // 10 minutes
    } else {
      if (retryNb < 5) {
        delay(random(500));
        retryNb++;
      } else {
        success = true;
      }
    }
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

  // Fetch and round temperature to one decimal
  float temperature = sensors.getTempC(dallasSensorAddress);

  // set power pin for DS18B20 to input before sleeping, saves power
  digitalWrite(PIN_ALIM_TEMPERATURE, LOW);
  pinMode(PIN_ALIM_TEMPERATURE, INPUT);

  return temperature;
}
