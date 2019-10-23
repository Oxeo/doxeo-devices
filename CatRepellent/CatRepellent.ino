#define MY_DEBUG

#define MY_RADIO_RF24
#define MY_RF24_PA_LEVEL (RF24_PA_MAX)
#define MY_TRANSPORT_MAX_TX_FAILURES (3u)

#include <MySensors.h>
#include <Parser.h>

#define SENSOR_ID 0
#define PUMP_PIN 5
#define PIR_PIN 3
#define LED_PIN A1
#define WATER_SENSOR_PIN A0

MyMessage msg(SENSOR_ID, V_CUSTOM);
Parser parser = Parser(' ');
bool _waterTankEmpty = false;
int _duration = 5;
int _spacing = 1;

void before()
{
  pinMode(PUMP_PIN, OUTPUT);
  pinMode(PIR_PIN, INPUT);
  pinMode(LED_PIN, OUTPUT);
  pinMode(WATER_SENSOR_PIN, INPUT);
  digitalWrite(PUMP_PIN, LOW);
  digitalWrite(LED_PIN, HIGH);
}

void setup()
{
  request(0, V_CUSTOM, 0);
  
  for (byte i=0; i<20; i++) {
    if (i%2 == 0) {
      digitalWrite(LED_PIN, LOW);
    } else {
      digitalWrite(LED_PIN, HIGH);
    }
    wait(100);
  }
  
  send(msg.set(F("cat repellent started")));
}

void presentation()
{
  sendSketchInfo("Cat Repellent", "1.0");
  present(SENSOR_ID, S_CUSTOM);
}

void receive(const MyMessage &myMsg)
{ 
  if (myMsg.type == V_CUSTOM && myMsg.sensor == 0) {
    Serial.print("New message: ");
    Serial.println(myMsg.getString());
    parser.parse(myMsg.getString());

    if (parser.get(0) != NULL && parser.get(1) != NULL) {
      _duration = parser.getInt(0);
      _spacing = parser.getInt(1);
      Serial.println ("Duration: " + _duration);
      Serial.println ("Spacing: " + _spacing);
      send(msg.set(F("configuration updated")));
    }
  }
}


void loop()
{
  if (digitalRead(PIR_PIN) == HIGH) {
    if (!isWaterTankEmpty()) {
      digitalWrite(PUMP_PIN, HIGH);
      digitalWrite(LED_PIN, HIGH);
      send(msg.set(F("cat alert")));
      wait(_duration * 1000);
      digitalWrite(PUMP_PIN, LOW);
      digitalWrite(LED_PIN, LOW);
      wait(_spacing * 1000);
    } else if (!_waterTankEmpty) {
      send(msg.set(F("water tank is empty")));
      _waterTankEmpty = true;
    }
  }

  sleep(digitalPinToInterrupt(PIR_PIN), RISING, 0);
}

bool isWaterTankEmpty() {
  pinMode(WATER_SENSOR_PIN, INPUT_PULLUP);
  sleep(15);
  bool result = digitalRead(WATER_SENSOR_PIN) == HIGH;
  pinMode(WATER_SENSOR_PIN, INPUT);
  return result;
}
