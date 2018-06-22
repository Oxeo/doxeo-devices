#include <SPI.h>
#include <Mirf.h>
#include <nRF24L01.h>
#include <MirfHardwareSpiDriver.h>

#include <LowPower.h>
#include <DoxeoConfig.h>

#define PIN_WAKEUP 2
#define PIN_ALIM 4

char* nodes[] = {DOXEO_ADDR_MOTHER, DOXEO_ADDR_SOUND};
const int nbNodes = 2;
int selectedNode = 0;

byte data[32];

void setup() {
  randomSeed(analogRead(0)); //initialise la séquence aléatoir
  
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
  Mirf.setTADDR((byte *) nodes[0]);

  selectedNode = 0;

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
  bool success;

  for (int i = 0; i < 3; ++i) {
    success = sendNrf(String(DOXEO_ADDR_DOORMAT) + ';' + String(DOXEO_ADDR_MOTHER) + ";" + msg);

    if (success) {
      break;
    } else {
      delay(random(100)); // avoid colision
    }
  }
}

bool sendNrf(String message) {
  message.getBytes(data, 32);

  for (unsigned char i = 0; i < nbNodes; ++i) {
    Mirf.send(data);
    while (Mirf.isSending());

    if (Mirf.sendWithSuccess == true) {
      break;
    } else {
      // change selected node
      selectedNode = (selectedNode + 1 < nbNodes) ? selectedNode + 1 : 0;
      Mirf.setTADDR((byte *) nodes[selectedNode]);
    }
  }

  Mirf.powerDown(); // power down NRF to save energy
  return Mirf.sendWithSuccess;
}

void wakeUp()
{
  // do nothing
}
