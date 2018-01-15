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
unsigned long stopTime = 0;

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
  Mirf.configRegister(RF_SETUP, 0x26); // sortie 0dBm @ 250Kbs to improve distance
  //Mirf.configRegister(CONFIG, 0x3F);

  sendMessage("init started");

  // init DFPlayer
  initDfPlayer();
  
  // play sound
  dfPlayer.volume(20);  //Set volume value. From 0 to 30
  dfPlayer.play(1);
  stopTime = millis() + 10*60000; // set active during 10 minutes

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
    } else if (message == "ping") {
      // send success message to the emitter
      sendMessage(String(id) + ";success");
      tokenId = id;
      tokenIdTime = millis();
    } else if (folder < 1 || folder > 99) {
      sendMessage(String(id) + ";folder parameter error (NAME;ID;folder-sound-volume)!");
    } else if (sound < 1 || sound > 999) {
      sendMessage(String(id) + ";sound parameter error (NAME;ID;folder-sound-volume)!");
    } else if (volume < 1 || volume > 30) {
      sendMessage(String(id) + ";volume parameter error (NAME;ID;folder-sound-volume)!");
    } else {
      // play sound
      digitalWrite(POWER_AMPLIFIER, LOW);
      dfPlayer.volume(volume);
      dfPlayer.playFolder(folder, sound);
      stopTime = millis() + 10*60000; // set active during 10 minutes
    
      // send success message to the emitter
      sendMessage(String(id) + ";success");
      tokenId = id;
      tokenIdTime = millis();
    }
  } else {
    // Sleep when timer elapsed
    if (stopTime < millis()) {
      dfPlayer.stop();
      //dfPlayer.sleep(); 
      digitalWrite(POWER_AMPLIFIER, HIGH);  // stop amplifier
      delay(100);
      if (!Mirf.dataReady()) {
        attachInterrupt(digitalPinToInterrupt(NRF_INTERRUPT), wakeUp, LOW);
        LowPower.powerDown(SLEEP_FOREVER, ADC_OFF, BOD_OFF);
        detachInterrupt(digitalPinToInterrupt(NRF_INTERRUPT));
        DEBUG_PRINT("WAKEUP");
        Mirf.configRegister(STATUS, 0x70); // clear IRQ register
      }
    } else {
        delay(10);
    }
  }

  // Get DFPlayer status
  if (dfPlayer.available()) {
    String msg = dfPlayerDetail(dfPlayer.readType(), dfPlayer.read());
    
    if (msg != "") {
        sendMessage(msg);
        
        if (dfPlayer.readType() == DFPlayerPlayFinished) {
            stopTime = millis() + 5000; // stop after 5 secondes
        }
    }
  }
}

void initDfPlayer() {
  dfPlayerSerial.begin(9600);

  sendMessage("Initializing DFPlayer ... (May take 3~5 seconds)");

  if (!dfPlayer.begin(dfPlayerSerial)) {
    sendMessage("Unable to initialize DFPlayer Mini, check the SD card!");
  }

  sendMessage("DFPlayer initialized");
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

String dfPlayerDetail(uint8_t type, int value) {
  switch (type) {
    case TimeOut:
      return "Time Out!";
    case WrongStack:
      return "Stack Wrong!";
    case DFPlayerCardInserted:
      return "Card Inserted!";
    case DFPlayerCardRemoved:
      return "Card Removed!";
    case DFPlayerCardOnline:
      return "Card Online!";
    case DFPlayerPlayFinished:
      return "Song " + String(value) + " play finished!";
    case DFPlayerError:
      switch (value) {
        case Busy:
          return "DFPlayerError: Card not found";
        case Sleeping:
          return "DFPlayerError: Sleeping";
        case SerialWrongStack:
          return "DFPlayerError: Get Wrong Stack";
        case CheckSumNotMatch:
          return "DFPlayerError: Check Sum Not Match";
        case FileIndexOut:
          return "DFPlayerError: File Index Out of Bound";
        case FileMismatch:
          return "DFPlayerError: Cannot Find File";
        case Advertise:
          return "DFPlayerError: In Advertise";
        default:
          return "";
      }
    default:
      return "";
  }
}

void wakeUp()
{
  // do nothing
}
