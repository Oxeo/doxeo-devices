#include "SoftwareSerial.h"
#include "DFRobotDFPlayerMini.h"
#include <SPI.h>      // Pour la communication via le port SPI
#include <Mirf.h>     // Pour la gestion de la communication
#include <nRF24L01.h> // Pour les définitions des registres du nRF24L01
#include <MirfHardwareSpiDriver.h> // Pour la communication SPI (ne cherchez pas à comprendre)
#include <LowPower.h>
#include <DoxeoConfig.h>

#define DEBUG
#include "DebugUtils.h"

#define DFPLAYER_RX_PIN 7
#define DFPLAYER_TX_PIN 8
#define POWER_DFPLAYER 6
#define POWER_AMPLIFIER 5
#define NRF_INTERRUPT 2

SoftwareSerial dfPlayerSerial(DFPLAYER_RX_PIN, DFPLAYER_TX_PIN);
DFRobotDFPlayerMini dfPlayer;

 /* 
  *  Pins:
  * Hardware SPI:
  * MISO -> 12
  * MOSI -> 11
  * SCK -> 13
  *
  * Configurable:
  * CE -> 9
  * CSN -> 10
  */

byte data[32];
int cpt = 0;

void setup() {
  pinMode(POWER_DFPLAYER, OUTPUT);
  pinMode(POWER_AMPLIFIER, OUTPUT);
  digitalWrite(POWER_DFPLAYER, HIGH);  // HIGH = ON
  digitalWrite(POWER_AMPLIFIER, LOW); // HIGH = OFF
  
#ifdef DEBUG
  Serial.begin(9600);
#endif

  // Configure NRF communication
//  Mirf.cePin = 9; // Broche CE sur D9
//  Mirf.csnPin = 10; // Broche CSN sur D10
//  Mirf.spi = &MirfHardwareSpi; // On veut utiliser le port SPI hardware
//  Mirf.init(); // Initialise la bibliothèque
//  Mirf.channel = DOXEO_CHANNEL; // Choix du canal de communication (128 canaux disponibles, de 0 à 127)
//  //Mirf.setRADDR((byte *) "nrf01"); // Adresse de réception
//  Mirf.payload = 32; // Taille d'un data (maximum 32 octets)
//  Mirf.config(); // Sauvegarde la configuration dans le module radio
//  Mirf.configRegister(RF_SETUP, 0x26); // sortie 0dBm @ 250Kbs to improve distance
//  Mirf.setRADDR((byte *) "soun1"); // Adresse de réception
//  Mirf.setTADDR((byte *) "oxeo2"); // Adresse de transmission

  sendMessage("init started");
  DEBUG_PRINT("init started");
  delay(100);
  sendMessage("init started2");

  // Configure DFPlayer
  
  sendMessage("init started3");
  delay(100);
  sendMessage("init started4");
  delay(1000);
  initDfPlayer();
  delay(1000);
  dfPlayer.volume(20);  //Set volume value. From 0 to 30
  dfPlayer.play(1);
  cpt = 100;

  sendMessage("init done");
  DEBUG_PRINT("init done");
}


void loop() {  
  // NRF reception
  if (false){//Mirf.dataReady()) {
    DEBUG_PRINT("dataReady");
    byte nrfMessage[32];
    Mirf.getData(nrfMessage);
    String message = (char*) nrfMessage;
    DEBUG_PRINT("message received:" + message);
    sendMessage("success");

    String folder = parseMsg(message, "-", 0);
    String sound = parseMsg(message, "-", 1);

    if (cpt < 1) {
      digitalWrite(POWER_DFPLAYER, HIGH);
      digitalWrite(POWER_AMPLIFIER, LOW);
      delay(1000);
      initDfPlayer();
      //dfPlayer.playFolder(folder.toInt(), sound.toInt());
      delay(1000);
      DEBUG_PRINT("play");
      dfPlayer.volume(15);
      dfPlayer.play(1);
      cpt = 10;
    }
  } else {    
    if (cpt < 1) {
      dfPlayer.stop();
      delay(1000);
      digitalWrite(POWER_DFPLAYER, LOW);
      digitalWrite(POWER_AMPLIFIER, HIGH);
      //attachInterrupt(digitalPinToInterrupt(NRF_INTERRUPT), wakeUp, LOW);
      LowPower.powerDown(SLEEP_FOREVER, ADC_OFF, BOD_OFF);
      //detachInterrupt(digitalPinToInterrupt(NRF_INTERRUPT));
    } else {
      DEBUG_PRINT("sleep");
      //LowPower.powerDown(SLEEP_1S, ADC_OFF, BOD_OFF);
      delay(1000);
      sendMessage("sleep");
      cpt--;
    }
  }

#ifdef DEBUG
 // if (dfPlayer.available()) {
   // printDfPlayerDetail(dfPlayer.readType(), dfPlayer.read()); //Print the detail message from DFPlayer to handle different errors and states.
  //}
#endif
}

void initDfPlayer() {
  dfPlayerSerial.begin(9600);

#ifdef DEBUG
  Serial.println(F("Initializing DFPlayer ... (May take 3~5 seconds)"));
#endif

  if (!dfPlayer.begin(dfPlayerSerial)) {  //Use softwareSerial to communicate with mp3.
    Serial.println(F("Unable to begin:"));
    Serial.println(F("1.Please recheck the connection!"));
    Serial.println(F("2.Please insert the SD card!"));
    sendMessage("Unable to initialize DFPlayer Mini, check the SD card!");
  }

#ifdef DEBUG
  Serial.println(F("DFPlayer Mini online."));
#endif
}

void sendMessage(String msg) {
//  String message = "sound;" + msg;
//  message.getBytes(data, 32);
//    Mirf.send(data);
//    while (Mirf.isSending());
}

String parseMsg(String data, char separator, int index)
{
  int found = 0;
  int strIndex[] = {0, -1};
  int maxIndex = data.length()-1;

  for(int i=0; i<=maxIndex && found<=index; i++){
    if(data.charAt(i)==separator || i==maxIndex){
        found++;
        strIndex[0] = strIndex[1]+1;
        strIndex[1] = (i == maxIndex) ? i+1 : i;
    }
  }

  return found>index ? data.substring(strIndex[0], strIndex[1]) : "";
}

void wakeUp()
{

}

void printDfPlayerDetail(uint8_t type, int value) {
  switch (type) {
    case TimeOut:
      Serial.println(F("Time Out!"));
      break;
    case WrongStack:
      Serial.println(F("Stack Wrong!"));
      break;
    case DFPlayerCardInserted:
      Serial.println(F("Card Inserted!"));
      break;
    case DFPlayerCardRemoved:
      Serial.println(F("Card Removed!"));
      break;
    case DFPlayerCardOnline:
      Serial.println(F("Card Online!"));
      break;
    case DFPlayerPlayFinished:
      Serial.print(F("Number:"));
      Serial.print(value);
      Serial.println(F(" Play Finished!"));
      break;
    case DFPlayerError:
      Serial.print(F("DFPlayerError:"));
      switch (value) {
        case Busy:
          Serial.println(F("Card not found"));
          break;
        case Sleeping:
          Serial.println(F("Sleeping"));
          break;
        case SerialWrongStack:
          Serial.println(F("Get Wrong Stack"));
          break;
        case CheckSumNotMatch:
          Serial.println(F("Check Sum Not Match"));
          break;
        case FileIndexOut:
          Serial.println(F("File Index Out of Bound"));
          break;
        case FileMismatch:
          Serial.println(F("Cannot Find File"));
          break;
        case Advertise:
          Serial.println(F("In Advertise"));
          break;
        default:
          break;
      }
      break;
    default:
      break;
  }
}

