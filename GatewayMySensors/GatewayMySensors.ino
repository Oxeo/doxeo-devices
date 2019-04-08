// Enable debug prints to serial monitor
#define MY_DEBUG

// Enable and select radio type attached
#define MY_RADIO_NRF24

// Set LOW transmit power level as default, if you have an amplified NRF-module and
// power your radio separately with a good regulator you can turn up PA level.
#define MY_RF24_PA_LEVEL (RF24_PA_MAX)

// Enable hardware signing
#define MY_SIGNING_ATSHA204
#define MY_SIGNING_REQUEST_SIGNATURES
#define MY_SIGNING_WEAK_SECURITY

// Enable serial gateway
#define MY_GATEWAY_SERIAL

// Enable IRQ pin
#define MY_RX_MESSAGE_BUFFER_FEATURE
#define MY_RF24_IRQ_PIN (2)
#define MY_RX_MESSAGE_BUFFER_SIZE (10)

// Enable inclusion mode
#define MY_INCLUSION_MODE_FEATURE

// Set inclusion mode duration (in seconds)
#define MY_INCLUSION_MODE_DURATION 60

#include <MySensors.h>

void setup()
{

}

void presentation()
{

}

void loop()
{
  //wait(10);
}

