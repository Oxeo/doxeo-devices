#include "SoftwareSerial.h"
#include "DFRobotDFPlayerMini.h"
#include <SPI.h>
#include <Mirf.h>
#include <nRF24L01.h>
#include <MirfHardwareSpiDriver.h>
#include <LowPower.h>
#include <DoxeoConfig.h>

//#define DEBUG
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
  Mirf.init();
  Mirf.channel = DOXEO_CHANNEL; // Choix du canal de communication (128 canaux disponibles, de 0 à 127)
  Mirf.setRADDR((byte *) DOXEO_ADDR_SOUND);
  Mirf.payload = 32; // Taille d'un data (maximum 32 octets)
  Mirf.config(); // Sauvegarde la configuration dans le module radio
  Mirf.configRegister(RF_SETUP, 0x26); // sortie 0dBm @ 250Kbs to improve distance
  Mirf.configRegister(EN_RXADDR, 0x02); // only pipe 1 can received
  Mirf.configRegister(SETUP_RETR, 0x3F);  // retry 15x

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
    if (receptorName == DOXEO_ADDR_MOTHER) {
        //redirect msg received to the motherboard
        sendNrf(msg);
    } else if (receptorName != DOXEO_ADDR_SOUND) {
      // do nothing, the message is not for us
    } else if (id == tokenId && (millis() - tokenIdTime < 60000)) {
      // already done, send success in case the previous message was not received
      sendAck(id);
    } else if (id == 0) {
      sendMessage("missing ID"); // NAME;ID;folder-sound-volume
    } else if (message == "ping") {
      sendAck(id);
    } else if (message == "stop") {
      sendAck(id);
      dfPlayer.stop();
      stopTime = millis() + 5000; // stop after 5 secondes
      sendMessage("Play stopped!");
    } else if (folder < 1 || folder > 99) {
      sendMessage(String(id) + ";folder arg error!");
    } else if (sound < 1 || sound > 999) {
      sendMessage(String(id) + ";sound arg error!");
    } else if (volume < 1 || volume > 30) {
      sendMessage(String(id) + ";volume arg error!");
    } else {
      sendAck(id);
      
      // play sound
      digitalWrite(POWER_AMPLIFIER, LOW);
      dfPlayer.volume(volume);
      dfPlayer.playFolder(folder, sound);
      stopTime = millis() + 10*60000; // set active during 10 minutes
    
      sendMessage("Play started!");
    }
  } else {
    // Sleep when timer elapsed
    if (stopTime < millis()) {
      dfPlayer.stop();
      digitalWrite(POWER_AMPLIFIER, HIGH);  // stop amplifier
      delay(50);
      if (!Mirf.dataReady()) {
        sleepForever();
      }
    }
  }

  // Get DFPlayer status
  if (dfPlayer.available()) {
    String msg = dfPlayerDetail(dfPlayer.readType(), dfPlayer.read());
    
    if (msg != "") {
        sendMessage(msg);
        
        if (msg == "Play finished!") {
            stopTime = millis() + 5000; // stop after 5 secondes
        }
    }
  }
}

void initDfPlayer() {
  dfPlayerSerial.begin(9600);

  if (!dfPlayer.begin(dfPlayerSerial)) {
    sendMessage("Init error check SD card");
  }
}

void sendAck(int id) {
  sendMessage(String(id) + ";success");
  tokenId = id;
  tokenIdTime = millis();
}

void sendMessage(String msg) {
  sendNrf(String(DOXEO_ADDR_SOUND) + ';' + msg);
}

void sendNrf(String message) {
  DEBUG_PRINT("send message: " + message);
  byte data[32];
  message.getBytes(data, 32);
  Mirf.setTADDR((byte *) DOXEO_ADDR_MOTHER);
  Mirf.configRegister(EN_RXADDR, 0x03); // only pipe 0 and 1 can received
  for (int i=0; i<10; ++i) {
    Mirf.send(data);
    while (Mirf.isSending());
    if (Mirf.sendWithSuccess == true) {
      break;
    }
  }
  Mirf.configRegister(EN_RXADDR, 0x02); // only pipe 1 can received
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

void sleepForever() {
  if (digitalRead(NRF_INTERRUPT) == LOW) {
    Mirf.configRegister(STATUS, 0x70); // clear IRQ register
  }
  
  attachInterrupt(digitalPinToInterrupt(NRF_INTERRUPT), wakeUp, LOW);
  LowPower.powerDown(SLEEP_FOREVER, ADC_OFF, BOD_OFF);
  detachInterrupt(digitalPinToInterrupt(NRF_INTERRUPT));
  DEBUG_PRINT("WAKEUP");
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
      return "Play finished!";
    case DFPlayerError:
      switch (value) {
        case Busy:
          return "Card not found";
        case Sleeping:
          return "Sleeping";
        case SerialWrongStack:
          return "Get Wrong Stack";
        case CheckSumNotMatch:
          return "Check Sum Not Match";
        case FileIndexOut:
          return "File Index Out of Bound";
        case FileMismatch:
          return "Cannot Find File";
        case Advertise:
          return "In Advertise";
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
