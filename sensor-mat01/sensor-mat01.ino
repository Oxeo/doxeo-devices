#include <SPI.h>
#include <Mirf.h>
#include <nRF24L01.h>
#include <MirfHardwareSpiDriver.h>

#include <LowPower.h>
#include <DoxeoConfig.h>

#define SENSOR_NAME "doormat"

#define PIN_WAKEUP 2
#define PIN_ALIM 4

byte data[32];
int sendError;

void setup() {
  sendError = 0;
  
  pinMode(PIN_WAKEUP, INPUT);
  pinMode(PIN_ALIM, OUTPUT);
  digitalWrite(PIN_ALIM, HIGH);
  
  // Configure NRF communication
  Mirf.cePin = 9; // Broche CE sur D9
  Mirf.csnPin = 10; // Broche CSN sur D10
  Mirf.spi = &MirfHardwareSpi;
  Mirf.init();
  Mirf.channel = 1; // Choix du canal de communication (128 canaux disponibles, de 0 à 127)
  Mirf.payload = 32; // Taille d'un data (maximum 32 octets)
  Mirf.config(); // Sauvegarde la configuration dans le module radio
  Mirf.configRegister(RF_SETUP, 0x26); // sortie 0dBm @ 250Kbs to improve distance
  Mirf.configRegister(SETUP_RETR, 0x3F);  // send retry 15 times
  Mirf.setTADDR((byte *) DOXEO_ADDR_MOTHER);

  sendMessage("init done");
}

void loop() {
  if (digitalRead(PIN_WAKEUP) == HIGH) {
    digitalWrite(PIN_ALIM, LOW);
    sendMessage("on");

    do {
      digitalWrite(PIN_ALIM, LOW);
      LowPower.powerDown(SLEEP_4S, ADC_OFF, BOD_OFF);
      digitalWrite(PIN_ALIM, HIGH);
      LowPower.powerDown(SLEEP_30MS, ADC_OFF, BOD_OFF);
    } while (digitalRead(PIN_WAKEUP) == HIGH);
  
    sendMessage("off");
  }
  
  // Sleep forever
  attachInterrupt(digitalPinToInterrupt(PIN_WAKEUP), wakeUp, CHANGE);
  LowPower.powerDown(SLEEP_FOREVER, ADC_OFF, BOD_OFF);
  detachInterrupt(digitalPinToInterrupt(PIN_WAKEUP));
}

void sendMessage(String msg) {
  bool success = sendNrf(String(SENSOR_NAME) + ';' + msg);

  if (success == false) {
    sendError++;
  } else if (sendError > 0) {
    if (sendNrf(String(SENSOR_NAME) + ";errorNb" + String(sendError))) {
      sendError = 0;
    }
  }
}

bool sendNrf(String message) {
  bool success = false;
  message.getBytes(data, 32);
  
  for (int i=0; i<10; ++i) {
    Mirf.send(data);
    while (Mirf.isSending());
    Mirf.powerDown(); // power down NRF to save energy
    
    if (Mirf.sendWithSuccess == true) {
      success = true;
      break;
    } else {
      LowPower.powerDown(SLEEP_120MS, ADC_OFF, BOD_OFF);
    }
  }

  return success;
}

void wakeUp()
{
  // do nothing
}
