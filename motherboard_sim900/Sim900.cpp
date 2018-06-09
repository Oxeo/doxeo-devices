#include "Sim900.h"

Sim900::Sim900(int rx, int tx) {
  state = 0;
  stateTimer = 0;
  error = "";
  info = "";
  smsToSend = {"", ""};
  newSmsReceived = false;
  newDataReceived = false;
  serial = new SoftwareSerial(rx, tx);
}

void Sim900::init() {
  serial->begin(9600);
  state = 100;
}

void Sim900::sendAtCmd(String message) {
  if (state == 0) {
    serial->print("WAKEUP\r");
    delay(100);
    readSIM900();
    serial->print(message);
  } else {
    error = "You need to wait the end of sending sms to send data";
  }
}

void Sim900::sendSms(String numbers, String message) {
  if (state == 0) {
    smsToSend.numbers = numbers;
    smsToSend.msg = message;
    state = 1;
  } else {
    error = "already sending an SMS";
  }
}

bool Sim900::newSms() {
  bool result = newSmsReceived;
  newSmsReceived = false;

  return result;
}

bool Sim900::newData() {
  bool result = newDataReceived;
  newDataReceived = false;

  return result;
}

char* Sim900::getData() {
  return buffer;
}

bool Sim900::isError() {
  return error.length() > 0;
}

String Sim900::getError() {
  String result = error;
  error = "";

  return result;
}

bool Sim900::isInfo() {
  return info.length() > 0;
}

String Sim900::getInfo() {
  String result = info;
  info = "";

  return result;
}

void Sim900::parseSms() {
  for (int i = 0; i < 100; i++) {
    buffer[i] = buffer[i + 9];
  }

  for (int i = 12; i < 100; i++) {
    buffer[i] = buffer[i + 5];
  }
  buffer[12] = '|';

  for (int i = 30; i < 100; i++) {
    buffer[i] = buffer[i + 5];
  }
  buffer[30] = '|';
}


void Sim900::update() {
  switch (state)
  {
    case 0:
      if (readSIM900()) {
        if (strContains (buffer, "+CMT:")) {
          parseSms();
          newSmsReceived = true;
        } else if (strContains (buffer, "Call Ready")) {
          error = "SIM900 has rebooting";
        } else {
          newDataReceived = true;
        }
      }
      break;
    case 1:
      info = "Sending SMS...";
      // WAKE UP
      serial->print("WAKEUP\r");
      state += 1;
      stateTimer = millis();
      break;
    case 2:
      // Wait to wake up
      if ((millis() - stateTimer) > 100) {
        readSIM900();
        state += 1;
      }
      break;
    case 3:
      // AT command to set SIM900 to SMS mode
      serial->print("AT+CMGF=1\r");
      state += 1;
      stateTimer = millis();
      break;
    case 4:
      // Wait OK
      if (readSIM900()) {
        if (strContains(buffer, "AT+CMGF") && strContains(buffer, "OK")) {
          state += 1;
        } else {
          error = "Unable to send SMS (mode error)";
          state = 0;
        }
      } else if ((millis() - stateTimer) > 500) {
        error = "Unable to send SMS (mode error): timeout";
        state = 0;
      }
      break;
    case 5:
      // mobile numbers
      serial->println("AT+CMGS = \"" + smsToSend.numbers + "\"");
      state += 1;
      stateTimer = millis();
      break;
    case 6:
      // Wait numbers
      if (readSIM900()) {
        if (strContains(buffer, const_cast<char*>(smsToSend.numbers.c_str()))) {
          state += 1;
        } else {
          error = "Unable to send SMS (numbers error)";
          state = 0;
        }
      } else if ((millis() - stateTimer) > 500) {
        error = "Unable to send SMS (numbers error): timeout";
        state = 0;
      }
      break;
    case 7:
      // message
      serial->println(smsToSend.msg);
      state += 1;
      stateTimer = millis();
      break;
    case 8:
      // Wait message
      if (readSIM900()) {
        if (strContains(buffer, const_cast<char*>(smsToSend.msg.c_str()))) {
          state += 1;
        } else {
          error = "Unable to send SMS (message error)";
          state = 0;
        }
      } else if ((millis() - stateTimer) > 500) {
        error = "Unable to send SMS (message error): timeout";
        state = 0;
      }
      break;
    case 9:
      // End AT command with a ^Z, ASCII code 26
      serial->println((char)26);
      state += 1;
      stateTimer = millis();
      break;
    case 10:
      // Wait OK
      if (readSIM900()) {
        if (strContains(buffer, "+CMGS:") && strContains(buffer, "OK")) {
          info = "SMS send with success";
        } else {
          error = "Unable to send SMS (send AT)";
        }
        state = 0;
      } else if ((millis() - stateTimer) > 5000) {
        error = "Unable to send SMS (send AT): timeout";
        state = 0;
      }
      break;
    case 100:
      info = "Initialize SIM900... (step 1)";
      state += 1;
      stateTimer = millis();
      break;
    case 101:
      // Wait 10s
      if ((millis() - stateTimer) > 1000) {
        state += 1;
      }
      break;
    case 102:
      // WAKE UP
      serial->print("WAKEUP\r");
      state += 1;
      stateTimer = millis();
      break;
    case 103:
      // Wait to wake up
      if ((millis() - stateTimer) > 100) {
        readSIM900();
        state += 1;
      }
      break;
    case 104:
      info = "Initialize SIM900... (step 2)";
      // Set SIM900 to SMS mode
      serial->print("AT+CMGF=1\r");
      state += 1;
      stateTimer = millis();
      break;
    case 105:
      // Wait OK
      if (readSIM900()) {
        if (strContains(buffer, "AT+CMGF=1") && strContains(buffer, "OK")) {
          state += 1;
        } else {
          error = "Unable to initialize SIM900 (mode error)";
          state = 0;
        }
      } else if ((millis() - stateTimer) > 500) {
        error = "Unable to initialize SIM900 (mode error): timeout";
        state = 0;
      }
      break;
    case 106:
      // Set module to send SMS data to serial out upon receipt
      serial->print("AT+CNMI=2,2,0,0,0\r");
      state += 1;
      stateTimer = millis();
      break;
    case 107:
      // Wait OK
      if (readSIM900()) {
        if (strContains(buffer, "AT+CNMI") && strContains(buffer, "OK")) {
          state += 1;
        } else {
          error = "Unable to initialize SIM900 (sms data)";
          state = 0;
        }
      } else if ((millis() - stateTimer) > 500) {
        error = "Unable to initialize SIM900 (sms data): timeout";
        state = 0;
      }
      break;
    case 108:
      // Enable sleep mode 2
      serial->print("AT+CSCLK=2\r");
      state += 1;
      stateTimer = millis();
      break;
    case 109:
      // Wait OK
      if (readSIM900()) {
        if (strContains(buffer, "AT+CSCLK=2") && strContains(buffer, "OK")) {
          info = "SIM900 initialized with success";
        } else {
          error = "Unable to initialize SIM900 (sleep mode)";
        }
        state = 0;
      } else if ((millis() - stateTimer) > 500) {
        error = "Unable to initialize SIM900 (sleep mode): timeout";
        state = 0;
      }
      break;
  }
}

bool Sim900::readSIM900() {
  if (serial->available()) {
    unsigned char cpt = 0;

    while (serial->available()) {
      buffer[cpt] = serial->read();
      cpt++;
      delay(5);

      if (cpt > 99) {
        break;
      }
    }

    buffer[cpt - 1] = 0; // end string
    return true;
  } else {
    return false;
  }
}

char Sim900::strContains(char *str, char *sfind)
{
  char found = 0;
  char index = 0;
  char len;

  len = strlen(str);

  if (strlen(sfind) > len) {
    return 0;
  }
  while (index < len) {
    if (str[index] == sfind[found]) {
      found++;
      if (strlen(sfind) == found) {
        return 1;
      }
    }
    else {
      found = 0;
    }
    index++;
  }

  return 0;
}

