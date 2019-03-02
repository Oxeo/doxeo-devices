// Enable debug prints to serial monitor
//#define MY_DEBUG

// Enable and select radio type attached
#define MY_RADIO_RF24

// Enable IRQ pin
#define MY_RX_MESSAGE_BUFFER_FEATURE
#define MY_RF24_IRQ_PIN (2)

// RF24 PA level
#define MY_RF24_PA_LEVEL (RF24_PA_MAX)

// Enable repeater functionality
#define MY_REPEATER_FEATURE

#include <MySensors.h>
#include <Keypad.h>

#define DFPLAYER_RX_PIN 8
#define DFPLAYER_TX_PIN 7
#define POWER_AMPLIFIER 5

MyMessage msg(0, V_CUSTOM);

char* password = "0000";
int position = 0;

const byte rows = 4; //four rows
const byte cols = 3; //three columns
char keys[rows][cols] = {
  {'1','2','3'},
  {'4','5','6'},
  {'7','8','9'},
  {'*','0','#'}
};
byte rowPins[rows] = {6, 5, 4, 3}; //connect to the row pinouts of the keypad
byte colPins[cols] = {A1, 8, 7}; //connect to the column pinouts of the keypad
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, rows, cols);

void before()
{
  // init PIN
  pinMode(POWER_AMPLIFIER, OUTPUT);
  digitalWrite(POWER_AMPLIFIER, HIGH);
}

void setup() {

}

void presentation() {
  // Send the sketch version information to the gateway and Controller
  sendSketchInfo("Siren", "1.0");

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

      send(msg.set(F("Play started")));
    }
  }
}

void loop() {
  char key = keypad.getKey();

  if (key != NO_KEY) {
    Serial.println(key);
    
    if (key == password[position]) {
      position++;
    } else {
      position = 0;
    }

    if (position == 4)
    {
      position = 0;
      //setLocked(false);
    }
  }
  
  wait(100);
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

