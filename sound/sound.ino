// Enable debug prints to serial monitor
#define MY_DEBUG

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
#define RELAY 5
#define LED A3

// DF Player
SoftwareSerial dfPlayerSerial(DFPLAYER_RX_PIN, DFPLAYER_TX_PIN);
DFRobotDFPlayerMini dfPlayer;

// Timer
unsigned long _previousMillis = 0;
unsigned long _timeToStayAwake = 0;

// State
enum State_enum {SLEEPING, WAITING, PLAYING};
uint8_t _state = SLEEPING;
String _messagePlaying = "";

// Led blink
boolean _ledOn = false;
unsigned long _previousLedChange = 0;

MyMessage msg(0, V_CUSTOM);

void before()
{
  // init PIN
  pinMode(RELAY, OUTPUT);
  digitalWrite(RELAY, LOW);
  pinMode(LED, OUTPUT);
  digitalWrite(LED, LOW);
}

void setup() {
  // init DFPlayer
  initDfPlayer();

  // play sound
  startAmplifier();
  dfPlayer.volume(20);  //Set volume value. From 0 to 30
  dfPlayer.play(1);
  changeState(PLAYING);
}

void presentation() {
  // Send the sketch version information to the gateway and Controller
  sendSketchInfo("Sound", "2.0");

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
      send(msg.set(F("play stopped")));
      changeState(WAITING);
    } else if (message == "ping") {
      send(msg.set(F("pong")));
    } else if (folder < 1 || folder > 99) {
      send(msg.set(F("folder arg error")));
    } else if (sound < 1 || sound > 999) {
      send(msg.set(F("sound arg error")));
    } else if (volume < 1 || volume > 30) {
      send(msg.set(F("volume arg error")));
    } else {
      if (_state != PLAYING || _messagePlaying != message || millis() - _previousMillis >= 10000) {
        // play sound
        if (_state == SLEEPING) {
          startAmplifier();
        }
        dfPlayer.volume(volume);
        dfPlayer.playFolder(folder, sound);
        _messagePlaying = message;
        changeState(PLAYING);
      } else {
        send(msg.set(F("already playing")));
      }
    }
  }
}

void loop() {
  if (_state != SLEEPING) {
    if (millis() - _previousMillis >= _timeToStayAwake) {
      changeState(SLEEPING);
    } else if (dfPlayer.available()) { // Get DFPlayer status
      char status = dfPlayerDetail(dfPlayer.readType(), dfPlayer.read());

      // Play finished
      if (status == 6) {
        if (_state == PLAYING) {
          send(msg.set(F("play finished")));
        }
        changeState(WAITING);
      }
    }

    // Blink led in waiting state
    if (_state == WAITING && (millis() - _previousLedChange >= 1000)) {
      _ledOn = !_ledOn;
      digitalWrite(LED, _ledOn);
      _previousLedChange = millis();
    }
  }
    
  wait(100);
}

void changeState(uint8_t state) {
  switch (state) {
    case PLAYING:
      _previousMillis = millis();
      _timeToStayAwake = 10 * 60000; // 10 minutes
      send(msg.set(F("play started")));
      break;
    case WAITING:
      _previousMillis = millis();
      _timeToStayAwake = 30000;
      break;
    case SLEEPING:
      stopAmplifier();
      send(msg.set(F("sleeping")));
      break;
  }

  _state = state;
}

void initDfPlayer() {
  dfPlayerSerial.begin(9600);

  if (!dfPlayer.begin(dfPlayerSerial)) {
    send(msg.set(F("Init error")));
  }

  dfPlayer.setTimeOut(500);
}

void startAmplifier() {
  digitalWrite(RELAY, HIGH);
  digitalWrite(LED, HIGH);
}

void stopAmplifier() {
  digitalWrite(RELAY, LOW);
  digitalWrite(LED, LOW);
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
      send(msg.set(F("time out")));
      return 1;
    case WrongStack:
      send(msg.set(F("wrong stack")));
      return 2;
    case DFPlayerCardInserted:
      send(msg.set(F("card inserted")));
      return 3;
    case DFPlayerCardRemoved:
      send(msg.set(F("card removed")));
      return 4;
    case DFPlayerCardOnline:
      send(msg.set(F("card online")));
      return 5;
    case DFPlayerPlayFinished:
      return 6;
    case DFPlayerError:
      switch (value) {
        case Busy:
          send(msg.set(F("card not found")));
          return 7;
        case Sleeping:
          send(msg.set(F("sleeping")));
          return 8;
        case SerialWrongStack:
          send(msg.set(F("get wrong ttack")));
          return 9;
        case CheckSumNotMatch:
          send(msg.set(F("checksum not match")));
          return 10;
        case FileIndexOut:
          send(msg.set(F("file index out of bound")));
          return 11;
        case FileMismatch:
          send(msg.set(F("cannot find file")));
          return 12;
        case Advertise:
          send(msg.set(F("in advertise")));
          return 13;
        default:
          send(msg.set(F("unknown error")));
          return 14;
      }
    default:
      return 0;
  }
}
