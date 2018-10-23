// Enable debug prints to serial monitor
//#define MY_DEBUG

// Enable and select radio type attached
#define MY_RADIO_NRF24

#define MY_RF24_PA_LEVEL (RF24_PA_MAX)

#include <MySensors.h>

#define PRIMARY_CHILD_ID 0

#define PRIMARY_BUTTON_PIN 2   // Arduino Digital I/O pin for button/reed switch

// Change to V_LIGHT if you use S_LIGHT in presentation below
MyMessage msg(PRIMARY_CHILD_ID, V_TRIPPED);

void setup()
{
  // Setup the buttons
  pinMode(PRIMARY_BUTTON_PIN, INPUT);

  // init PIN
  pinMode(A0, INPUT);
}

void presentation()
{
  sendSketchInfo("Door 2", "1.0");
  present(PRIMARY_CHILD_ID, S_DOOR);
}

// Loop will iterate on changes on the BUTTON_PINs
void loop()
{
  uint8_t value;
  static uint8_t sentValue = 2;

  // Short delay to allow buttons to properly settle
  delay(50);

  value = digitalRead(PRIMARY_BUTTON_PIN);

  if (value != sentValue) {
    // Value has changed from last transmission, send the updated value
    send(msg.set(value == HIGH));
    sentValue = value;
  }

  // Sleep until something happens with the sensor
  sleep(PRIMARY_BUTTON_PIN - 2, CHANGE, 0);
}
