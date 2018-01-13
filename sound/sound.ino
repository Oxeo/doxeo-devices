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

#define DFPLAYER_RX_PIN 8
#define DFPLAYER_TX_PIN 7
#define POWER_AMPLIFIER 5
#define NRF_INTERRUPT 2

// DF Player
SoftwareSerial dfPlayerSerial(DFPLAYER_RX_PIN, DFPLAYER_TX_PIN);
DFRobotDFPlayerMini dfPlayer;

// Timer
int timer = 0;

// Token ID
unsigned long tokenId = 0;
unsigned long tokenIdTime = 0;

void setup() {
  // init PIN
  pinMode(NRF_INTERRUPT, INPUT);
  pinMode(POWER_AMPLIFIER, OUTPUT);
  digitalWrite(POWER_AMPLIFIER, LOW); // HIGH = OFF
  
  // init serial for debugging
  #ifdef DEBUG
    Serial.begin(9600);
  #endif

  // init NRF
  Mirf.cePin = 9; // PIN CE sur D9
  Mirf.csnPin = 10; // PIN CSN sur D10
  Mirf.spi = &MirfHardwareSpi; // Hardware SPI: MISO -> 12, MOSI -> 11, SCK -> 13
  Mirf.init(); // Initialise la bibliothèque
  Mirf.channel = DOXEO_CHANNEL; // Choix du canal de communication (128 canaux disponibles, de 0 à 127)
  Mirf.setRADDR((byte *) DOXEO_ADDR_SOUND); // Adresse de réception
  Mirf.payload = 32; // Taille d'un data (maximum 32 octets)
  Mirf.config(); // Sauvegarde la configuration dans le module radio
  //Mirf.configRegister(RF_SETUP, 0x26); // sortie 0dBm @ 250Kbs to improve distance

  sendMessage("init started");

  // init DFPlayer
  initDfPlayer();
  
  // play sound
  dfPlayer.volume(20);  //Set volume value. From 0 to 30
  dfPlayer.play(1);
  timer = 20; //60*3; // set active 3 minutes

  sendMessage("init done");
}


void loop() {  
  // message received
  if (Mirf.dataReady()) {
    // read message
    byte byteMessage[32];
    Mirf.getData(byteMessage);
    String msg = String((char*) byteMessage);
    DEBUG_PRINT("message received:" + msg);

    // parse message
    String receptorName = parseMsg(msg,';', 0);
    int id = parseMsg(msg, ';', 1).toInt();
    String message = parseMsg(msg, ';', 2);
    int folder = parseMsg(message, '-', 0).toInt();
    int sound = parseMsg(message, '-', 1).toInt();
    int volume = parseMsg(message, '-', 2).toInt();
    
    // display message for debug
    //DEBUG_PRINT("receptorName: " + receptorName);
    //DEBUG_PRINT("id: " + String(id));
    //DEBUG_PRINT("folder: " + String(folder));
    //DEBUG_PRINT("sound: " + String(sound));
    //DEBUG_PRINT("volume: " + String(volume));

    // handle message
    if (receptorName != DOXEO_ADDR_SOUND) {
      // do nothing, the message is not for us
    } else if (id == tokenId && (millis() - tokenIdTime < 60000)) {
      // already done, send success in case the previous message was not received
      sendMessage(String(id) + ";success");
    } else if (id == 0) {
      sendMessage("missing ID (NAME;ID;folder-sound-volume)");
    } else if (folder < 1 || folder > 99) {
      sendMessage(String(id) + ";folder parameter error (NAME;ID;folder-sound-volume)!");
    } else if (sound < 1 || sound > 999) {
      sendMessage(String(id) + ";sound parameter error (NAME;ID;folder-sound-volume)!");
    } else if (volume < 1 || volume > 30) {
      sendMessage(String(id) + ";volume parameter error (NAME;ID;folder-sound-volume)!");
    } else {
      // play sound
      digitalWrite(POWER_AMPLIFIER, LOW);
      delay(500);
      dfPlayer.volume(volume);
      dfPlayer.playFolder(folder, sound);
      timer = 10;//60*3;  // set active during 3 minutes
      
      // send success message to the emitter
      sendMessage(String(id) + ";success");
      tokenId = id;
      tokenIdTime = millis();
    }
  } else {
    // Sleep when timer elapsed
    if (timer < 1) {
      dfPlayer.stop();
      //dfPlayer.sleep(); 
      digitalWrite(POWER_AMPLIFIER, HIGH);  // stop amplifier
      delay(500);
      attachInterrupt(digitalPinToInterrupt(NRF_INTERRUPT), wakeUp, FALLING);
      LowPower.powerDown(SLEEP_FOREVER, ADC_OFF, BOD_OFF);
      detachInterrupt(digitalPinToInterrupt(NRF_INTERRUPT));
      DEBUG_PRINT("WAKEUP");
    } else {
      #ifdef DEBUG
        delay(1000);
      #else
        LowPower.powerDown(SLEEP_1S, ADC_OFF, BOD_OFF);
      #endif
      
      timer--;
    }
  }

  #ifdef DEBUG
    if (dfPlayer.available()) {
      printDfPlayerDetail(dfPlayer.readType(), dfPlayer.read());
    }
  #endif
}

void initDfPlayer() {
  dfPlayerSerial.begin(9600);

  sendMessage("Initializing DFPlayer ... (May take 3~5 seconds)");

  if (!dfPlayer.begin(dfPlayerSerial)) {  //Use softwareSerial to communicate with mp3.
    Serial.println(F("Unable to begin:"));
    Serial.println(F("1.Please recheck the connection!"));
    Serial.println(F("2.Please insert the SD card!"));
    sendMessage("Unable to initialize DFPlayer Mini, check the SD card!");
  }

  sendMessage("DFPlayer Mini online");
}

void sendMessage(String msg) {
  String message = String(DOXEO_ADDR_SOUND) + ';' + msg;
  DEBUG_PRINT("send message: " + message);
  byte data[32];
  message.getBytes(data, 32);
  Mirf.setTADDR((byte *) DOXEO_ADDR_MOTHER); // Adresse de transmission
  Mirf.send(data);
  while (Mirf.isSending());
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

void wakeUp()
{
  // do nothing
}
