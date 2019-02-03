// Enable debug prints to serial monitor
//#define MY_DEBUG

// Enable and select radio type attached
#define MY_RADIO_RF24

// Enable IRQ pin
#define MY_RX_MESSAGE_BUFFER_FEATURE
#define MY_RF24_IRQ_PIN (2)

// RF24 PA level
#define MY_RF24_PA_LEVEL (RF24_PA_HIGH)

// Enable repeater functionality
#define MY_REPEATER_FEATURE

#include <MySensors.h>
#include "SoftwareSerial.h"
#include "DFRobotDFPlayerMini.h"

#define DFPLAYER_RX_PIN 8
#define DFPLAYER_TX_PIN 7
#define POWER_AMPLIFIER 5

// DF Player
SoftwareSerial dfPlayerSerial(DFPLAYER_RX_PIN, DFPLAYER_TX_PIN);
DFRobotDFPlayerMini dfPlayer;

// Timer
unsigned long sleepTimer = 0;
bool isOn = false;

MyMessage msg(0, V_CUSTOM);

void before()
{
  // init PIN
  pinMode(POWER_AMPLIFIER, OUTPUT);
  digitalWrite(POWER_AMPLIFIER, HIGH);
}

void setup() {
  // init DFPlayer
  initDfPlayer();

  // play sound
  dfPlayer.volume(20);  //Set volume value. From 0 to 30
  dfPlayer.play(1);
  isOn = true;
  sleepTimer = millis() + 10 * 60000; // set active during 10 minutes
}

void presentation() {
  // Send the sketch version information to the gateway and Controller
  sendSketchInfo("Sound", "1.0");

  // Present sensor to controller
  present(0, S_CUSTOM);
}

void receive(const MyMessage &myMsg)
{
  if (myMsg.type == V_CUSTOM) {
    String message = myMsg.getString();

    // parse message
    int folder = parseMsg(message, '-', 0).toInt();
    int sound = parseMsg(message, '-', 1).toInt();
    int volume = parseMsg(message, '-', 2).toInt();

    if (message == "stop") {
      dfPlayer.stop();
    } else if (message == "ping") {
      send(msg.set(F("pong")));
    } else if (folder < 1 || folder > 99) {
      send(msg.set(F("folder arg error")));
    } else if (sound < 1 || sound > 999) {
      send(msg.set(F("sound arg error")));
    } else if (volume < 1 || volume > 30) {
      send(msg.set(F("volume arg error")));
    } else {
      // play sound
      digitalWrite(POWER_AMPLIFIER, HIGH);
      dfPlayer.volume(volume);
      dfPlayer.playFolder(folder, sound);
      isOn = true;
      sleepTimer = millis() + 10 * 60000; // set active during 10 minutes

      send(msg.set(F("Play started")));
    }
  }
}

void loop() {
  if (isOn) {
    if (sleepTimer < millis()) {
      digitalWrite(POWER_AMPLIFIER, LOW);  // stop amplifier
      isOn = false;
    } else if (dfPlayer.available()) { // Get DFPlayer status
      char errorNb = dfPlayerDetail(dfPlayer.readType(), dfPlayer.read());

      // Play finished
      if (errorNb == 6) {
        sleepTimer = millis() + 5000; // stop after 5 secondes
      }
    }
    
    wait (500);
  } else {
    wait(0);
  }
}

void initDfPlayer() {
  dfPlayerSerial.begin(9600);

  if (!dfPlayer.begin(dfPlayerSerial)) {
    send(msg.set(F("Init error")));
  }
}

String parseMsg(String data, char separator, int index) {
  int found = 0;
  int strIndex[] = {0, -1};
  int maxIndex = data.length() - 1;

  for (int i = 0; i <= maxIndex && found <= index; i++) {
    if (data.charAt(i) == separator || i == maxIndex) {
      found++;
      strIndex[0] = strIndex[1] + 1;
      strIndex[1] = (i == maxIndex) ? i + 1 : i;
    }
  }

  return found > index ? data.substring(strIndex[0], strIndex[1]) : "";
}

char dfPlayerDetail(uint8_t type, int value) {
  switch (type) {
    case TimeOut:
      send(msg.set(F("Time Out")));
      return 1;
    case WrongStack:
      send(msg.set(F("Stack Wrong")));
      return 2;
    case DFPlayerCardInserted:
      send(msg.set(F("Card Inserted")));
      return 3;
    case DFPlayerCardRemoved:
      send(msg.set(F("Card Removed")));
      return 4;
    case DFPlayerCardOnline:
      send(msg.set(F("Card Online")));
      return 5;
    case DFPlayerPlayFinished:
      send(msg.set(F("Play finished")));
      return 6;
    case DFPlayerError:
      switch (value) {
        case Busy:
          send(msg.set(F("Card not found")));
          return 7;
        case Sleeping:
          send(msg.set(F("Sleeping")));
          return 8;
        case SerialWrongStack:
          send(msg.set(F("Get Wrong Stack")));
          return 9;
        case CheckSumNotMatch:
          send(msg.set(F("Check Sum Not Match")));
          return 10;
        case FileIndexOut:
          send(msg.set(F("File Index Out of Bound")));
          return 11;
        case FileMismatch:
          send(msg.set(F("Cannot Find File")));
          return 12;
        case Advertise:
          send(msg.set(F("In Advertise")));
          return 13;
        default:
          send(msg.set(F("Unknown error")));
          return 14;
      }
    default:
      return 0;
  }
}
