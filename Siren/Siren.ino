// Enable debug prints to serial monitor
#define MY_DEBUG

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

#define SIREN A0
#define BUZZER A2
#define BLUE_LED A3
#define GREEN_LED A4
#define RED_LED A5

#if defined(MY_DEBUG)
  #define DEBUG_PRINT(str) Serial.println(str);
#else
  #define DEBUG_PRINT(str)
#endif

MyMessage msg(0, V_CUSTOM);

char* password = "0000";
int position = 0;
unsigned long previousKeyPressed = 0;
unsigned long positionResetTime = 30000L;
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

unsigned long sirenTime = 0;
char sirenState = 0;
char bipNumber = 6;

void before()
{
  // init PIN
  pinMode(SIREN, OUTPUT);
  digitalWrite(SIREN, LOW);
  pinMode(BUZZER, OUTPUT);
  digitalWrite(BUZZER, LOW);
  pinMode(BLUE_LED, OUTPUT);
  digitalWrite(BLUE_LED, LOW);
  pinMode(GREEN_LED, OUTPUT);
  digitalWrite(GREEN_LED, LOW);
  pinMode(RED_LED, OUTPUT);
  digitalWrite(RED_LED, LOW);

  enableSiren(false);
}

void setup() {
  attachInterrupt(digitalPinToInterrupt(rowPins[rows-1]), keyboardInterrupt, FALLING);
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
    String arg1 = parseMsg(message, '-', 0);
    String arg2 = parseMsg(message, '-', 1);

    if (message == "ping") {
      send(msg.set(F("pong")));
    } else if (message == "stop") {
      if (isSirenOn()) {
        enableSiren(false);
        send(msg.set(F("stopped by user")));
      }
    } else if (arg1 == "start") {
      bipNumber = arg2.toInt();
      enableSiren(true);
      send(msg.set(F("started")));
    } else {
      send(msg.set(F("args error")));
    }
  }
}

void loop() {
  char key = keypad.getKey();

  if (key != NO_KEY) {
    DEBUG_PRINT(key);
    previousKeyPressed = millis();
    
    if (key == password[position]) {
      position++;
    } else {
      DEBUG_PRINT(F("Wrong password!"));
      position = 0;
    }

    if (position == 4)
    {
      DEBUG_PRINT(F("Correct password entered!"));
      position = 0;

      if (isSirenOn()) {
        enableSiren(false);
        send(msg.set(F("stopped by password")));
      } else {
        sound(true);
        delayMicroseconds(100);
        sound(false);
        send(msg.set(F("password entered")));
      }
    }
  }

  if(millis() - previousKeyPressed >= positionResetTime && !isSirenOn()) {
    DEBUG_PRINT(F("Sleep!"));
    position = 0;
    waitUntilKeyPressed();
    DEBUG_PRINT(F("Wake up!"));
    DEBUG_PRINT(bipNumber);
  } else {
    manageSiren();
    wait(100);
  }
}

void enableSiren(bool enable) {
  if (enable) {
    sirenTime = 0;
    sirenState = 1;
    DEBUG_PRINT(F("Siren started!"));
  } else {
    sirenState = 0;
    sound(false);
  }
}

bool isSirenOn() {
  return sirenState != 0;
}

void manageSiren() {
  if (sirenState == 0) {
    // nothing to do
  } else if ((sirenState % 2 == 1 && sirenState < bipNumber * 2 + 2) && millis() - sirenTime >= 1000) {
    sound(true);
    sirenTime = millis();
    sirenState++;
  } else if ((sirenState % 2 == 0 && sirenState < bipNumber * 2 + 2) && millis() - sirenTime >= 500) {
    sound(false);
    sirenTime = millis();
    sirenState++;
  } else if ((sirenState == bipNumber * 2 + 2) && millis() - sirenTime >= 60000) {
    DEBUG_PRINT(F("Siren stopped after 60s!"));
    enableSiren(false);
    send(msg.set(F("stopped")));
  }
}

void sound(bool enable) {
  if (enable) {
      digitalWrite(SIREN, HIGH);
  } else {
      digitalWrite(SIREN, LOW);
  }
}

void waitUntilKeyPressed() {
  for(char i=0; i<cols; i++) {
    pinMode(colPins[i], OUTPUT);
    digitalWrite(colPins[i], LOW);
  }

  pinMode(rowPins[rows-1], INPUT_PULLUP);
  wait(600000);
}

void keyboardInterrupt() {
  // nothing to do
  bipNumber++;
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

