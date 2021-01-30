// Enable debug prints to serial monitor
#define MY_DEBUG

// Enable and select radio type attached
#define MY_RADIO_RF24

// Set LOW transmit power level as default, if you have an amplified NRF-module and
// power your radio separately with a good regulator you can turn up PA level.
#define MY_RF24_PA_LEVEL (RF24_PA_HIGH)

// Enable hardware signing
#define MY_SIGNING_ATSHA204
#define MY_SIGNING_REQUEST_SIGNATURES
#define MY_SIGNING_WEAK_SECURITY

// Enable serial gateway
#define MY_GATEWAY_SERIAL

// Enable IRQ pin
#define MY_RX_MESSAGE_BUFFER_FEATURE
#define MY_RF24_IRQ_PIN (2)
#define MY_RX_MESSAGE_BUFFER_SIZE (8)

#include <MySensors.h>

MyMessage msgToRelay;
unsigned long relayTimer = 0;
MyMessage msg(0, V_CUSTOM);

void setup()
{

}

void presentation()
{

}

void receive(const MyMessage &message)
{
  if (message.type == V_CUSTOM && message.sensor == 0 && message.sender == 0) {
    relayMessage(&message);
  }
}

void loop()
{
  wait(5);
}

void relayMessage(const MyMessage *message) {
  if (getPayload(message->getString()) != NULL) {
      msgToRelay.destination = getMessagePart(message->getString(), 0);
      msgToRelay.sensor = getMessagePart(message->getString(), 1);
      mSetCommand(msgToRelay, getMessagePart(message->getString(), 2));
      mSetRequestAck(msgToRelay, getMessagePart(message->getString(), 3));
      msgToRelay.type = getMessagePart(message->getString(), 4);
      msgToRelay.sender = message->sender;
      mSetAck(msgToRelay, false);
      msgToRelay.set(getPayload(message->getString()));

      relayTimer = millis();
      bool success = false;
      while(millis() - relayTimer < 2500 && !success) {
        success = transportSendWrite(msgToRelay.destination, msgToRelay);
        wait(5);
      }

      if (success) {
        send(msg.set(F("SUCCESS")));
      } else {
        send(msg.set(F("KO")));
      }
    }
}

int getMessagePart(const char* message, const byte index) {
  byte indexCount = 0;

  if (index == 0 && strlen(message) > 0) {
    return atoi(message);
  }
  
  for (byte i=0; i < strlen(message) - 1; i++) {
    if (message[i] == '-') {
      indexCount++;
    }

    if (indexCount == index) {
      return atoi(message + i + 1);
    }
  }

  return 0;
}

char* getPayload(const char* message) {
  byte indexCount = 0;
  
  for (byte i=0; i < strlen(message) - 1; i++) {
    if (message[i] == '-') {
      indexCount++;
    }

    if (indexCount == 5) {
      return message + i + 1;
    }
  }

  return NULL;
}
