#define MY_DEBUG
#define MY_RADIO_RF24
#define MY_RF24_PA_LEVEL (RF24_PA_MAX)

#define REPORT_BATTERY_LEVEL

#include <MySensors.h>
#include <Wire.h>  // A4 => SDA, A5 => SCL

#define I2C_ADDR 0x5A // Adresse par défaut du capteur IR
#define TEMP_ID 0
#define TARGET_ID 1
#define STATE_ID 2
#define TEMP_POWER A0
#define RED_LED_PIN 3
#define GREEN_LED_PIN 4
#define BLUE_LED_PIN 5
#define EEPROM_TARGET_TEMP 0

MyMessage temperatureMsg(TEMP_ID, V_TEMP);
MyMessage stateMsg(STATE_ID, V_TEXT);

enum State_enum {SLEEPING, STARTING, HOT, READY, COLD};

int targetTemperature = -100;
int oldTemperature = -100;
uint8_t state = SLEEPING;
unsigned long coldCpt = 0;

#ifdef REPORT_BATTERY_LEVEL
#include <Vcc.h>
static uint8_t oldBatteryPcnt = 200;  // Initialize to 200 to assure first time value will be sent.
const float VccMin        = 2.8;      // Minimum expected Vcc level, in Volts: Brownout at 2.8V    -> 0%
const float VccMax        = 2.0 * 1.6; // Maximum expected Vcc level, in Volts: 2xAA fresh Alkaline -> 100%
const float VccCorrection = 1.0;      // Measured Vcc by multimeter divided by reported Vcc
static Vcc vcc(VccCorrection);
#endif

void before()
{
  pinMode(RED_LED_PIN, OUTPUT);
  pinMode(GREEN_LED_PIN, OUTPUT);
  pinMode(BLUE_LED_PIN, OUTPUT);

  digitalWrite(RED_LED_PIN, HIGH);
  wait(500);
  digitalWrite(RED_LED_PIN, LOW);
  digitalWrite(GREEN_LED_PIN, HIGH);
  wait(500);
  digitalWrite(GREEN_LED_PIN, LOW);
  digitalWrite(BLUE_LED_PIN, HIGH);
  wait(500);
  digitalWrite(BLUE_LED_PIN, LOW);
  
  pinMode(RED_LED_PIN, INPUT);
  pinMode(GREEN_LED_PIN, INPUT);
  pinMode(BLUE_LED_PIN, INPUT);
}

void presentation()
{
  sendSketchInfo("Hot Tea Alert", "1.0");
  present(TEMP_ID, S_TEMP, "Tea temperature");
  present(TARGET_ID, S_TEMP, "Target temperature");
  present(STATE_ID, S_INFO, "State status");
}

void setup()
{
  state = SLEEPING;
  coldCpt = 0;
  oldTemperature = -100;
  targetTemperature = getTargetTemperature();

  // Initialisation du bus I2C
  Wire.begin();

#ifdef MY_DEBUG
  Serial.print(F("Target Temperature in EEPROM: "));
  Serial.println(targetTemperature);
#endif

  request(TARGET_ID, V_TEMP); // request target temperature
  wait(4000); // wait 4 seconds for answer


#ifdef REPORT_BATTERY_LEVEL
  reportBatteryLevel();
#endif
}

void loop()
{
  int temperature = round(getTemperature());

#ifdef MY_DEBUG
  Serial.print(F("Temperature: "));
  Serial.println(temperature);
#endif

  if (state == SLEEPING) {
    if (temperature < 40) {
      sleep(30000); // 30 seconds
    } else {
      pinMode(RED_LED_PIN, OUTPUT);
      pinMode(GREEN_LED_PIN, OUTPUT);
      pinMode(BLUE_LED_PIN, OUTPUT);
      oldTemperature = -100;
      state = STARTING;
      #ifdef MY_DEBUG
        Serial.print(F("State: STARTING"));
      #endif
    }
  } else {
      if (oldTemperature != temperature) {
        send(temperatureMsg.set(temperature));
        oldTemperature = temperature;

        if (temperature > targetTemperature && state != HOT) {
          digitalWrite(RED_LED_PIN, HIGH);
          digitalWrite(GREEN_LED_PIN, LOW);
          digitalWrite(BLUE_LED_PIN, LOW);
          send(stateMsg.set("HOT"));
          state = HOT;
          #ifdef MY_DEBUG
            Serial.print(F("State: HOT"));
          #endif
        } else if (temperature <= targetTemperature
                   && temperature >= targetTemperature - 10
                   && state != READY) {
          digitalWrite(RED_LED_PIN, LOW);
          digitalWrite(GREEN_LED_PIN, HIGH);
          digitalWrite(BLUE_LED_PIN, LOW);
          send(stateMsg.set("READY"));
          state = READY;
          #ifdef MY_DEBUG
            Serial.print(F("State: READY"));
          #endif
        } else if (temperature < targetTemperature - 10 && state != COLD) {
          digitalWrite(RED_LED_PIN, LOW);
          digitalWrite(GREEN_LED_PIN, LOW);
          digitalWrite(BLUE_LED_PIN, HIGH);
          send(stateMsg.set("COLD"));
          coldCpt = 0;
          state = COLD;
          #ifdef MY_DEBUG
            Serial.print(F("State: COLD"));
          #endif
        }
      }

      if (state == COLD) {
        coldCpt++;
      }

      if (coldCpt >= 600) {  // sleep after 10 minutes
        digitalWrite(RED_LED_PIN, LOW);
        digitalWrite(GREEN_LED_PIN, LOW);
        digitalWrite(BLUE_LED_PIN, LOW);
        pinMode(RED_LED_PIN, INPUT);
        pinMode(GREEN_LED_PIN, INPUT);
        pinMode(BLUE_LED_PIN, INPUT);
        send(stateMsg.set("SLEEPING"));
        state = SLEEPING;
        #ifdef MY_DEBUG
            Serial.print(F("State: SLEEPING"));
        #endif
      } else {
        sleep(1000);
      }
    }
}

void receive(const MyMessage &myMsg)
{
  if (myMsg.sensor == TARGET_ID && myMsg.type == V_TEMP) {
    int newTarget = myMsg.getInt();

#ifdef MY_DEBUG
    Serial.print(F("Target Temperature received: "));
    Serial.println(newTarget);
#endif

    if (newTarget >= 30 && newTarget <= 200 && targetTemperature != newTarget) {
      targetTemperature = newTarget;
      //saveState(EEPROM_TARGET_TEMP, newTarget);
      
      pinMode(RED_LED_PIN, OUTPUT);
      for (char i = 0; i < 10; i++) {
        if (i % 2 == 0) {
          digitalWrite(RED_LED_PIN, HIGH);
        } else {
          digitalWrite(RED_LED_PIN, LOW);
        }

        delay(50);
      }
      pinMode(RED_LED_PIN, INPUT);
    }
  }
}

int getTargetTemperature() {
  int result = loadState(EEPROM_TARGET_TEMP);

  if (targetTemperature < 30 || targetTemperature > 200) {
    result = 70;
  }
  
  return result;
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
  const uint8_t batteryPcnt = static_cast<uint8_t>(0.5 + vcc.Read_Perc(VccMin, VccMax));

#ifdef MY_DEBUG
  Serial.print(F("Vbat "));
  Serial.print(vcc.Read_Volts());
  Serial.print(F("\tPerc "));
  Serial.println(batteryPcnt);
#endif

  // Battery readout should only go down. So report only when new value is smaller than previous one.
  if ( batteryPcnt < oldBatteryPcnt )
  {
    sendBatteryLevel(batteryPcnt);
    oldBatteryPcnt = batteryPcnt;
  }
}

