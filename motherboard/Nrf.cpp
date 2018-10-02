#include "Arduino.h"
#include "nrf.h"

bool Nrf::_newMsgReceived = false;

Nrf::Nrf(int pinInterrupt)
{
  _pinInterrupt = pinInterrupt;
}

void Nrf::init()
{
  pinMode(_pinInterrupt, INPUT);
  _sendQueue.setPrinter (Serial);

  Mirf.cePin = 9;
  Mirf.csnPin = 10;
  Mirf.spi = &MirfHardwareSpi; // Hardware SPI: MISO -> 12, MOSI -> 11, SCK -> 13
  Mirf.init();
  Mirf.channel = DOXEO_CHANNEL; // Choix du canal de communication (128 canaux disponibles, de 0 à 127)
  Mirf.setRADDR((byte *) DOXEO_ADDR_MOTHER);
  Mirf.payload = 32; // Taille d'un message (maximum 32 octets)
  Mirf.config(); // Sauvegarde la configuration dans le module radio
  Mirf.configRegister(RF_SETUP, 0x26); // to send much longeur
  Mirf.configRegister(SETUP_RETR, 0x3F);  // retry 15x

  if (digitalRead(_pinInterrupt) == LOW) {
    Mirf.configRegister(STATUS, 0x70); // clear IRQ register
  }
  attachInterrupt(digitalPinToInterrupt(_pinInterrupt), Nrf::interruptHandler, FALLING);

  _emergencySending = false;
  _sendId = 1;
  _tryNumber = 0;
  _remainingSend = 0;
  _ackReceived = false;
}

void Nrf::sendMessage(String message)
{
  _sendQueue.push(message);
}

void Nrf::update()
{
  checkNewMessage();
  unstackMessageToSend();
  sendProcess();
}

void Nrf::checkNewMessage()
{
  if (_newMsgReceived) {
    _newMsgReceived = false;
    
    while (Mirf.dataReady()) {
      byte byteMsg[32];
      Mirf.getData(byteMsg);
      String message = String((char *)byteMsg);

      // success returned no need to send again
      if (message == _successMsgExpected) {
        _emergencySending = false;
        _remainingSend = 0;
        Serial.println("success received");
      } else {
        char destAddressIndex = message.indexOf(String(DOXEO_ADDR_MOTHER) + ";");
        if (destAddressIndex > 0) {
          // remove destination address
          message.remove(destAddressIndex, 6);
        }
        Serial.println("nrf;" + message);
      }
    };

    
    if (digitalRead(_pinInterrupt) == LOW) {
      Mirf.configRegister(STATUS, 0x70); // clear IRQ register
    }
  }
}

void Nrf::unstackMessageToSend()
{
  if (_remainingSend == 0 && !_sendQueue.isEmpty()) {
    String queue = _sendQueue.pop();

    // Set destination address
    byte nrfByteAddress[6];
    parseCommand(queue, ';', 1).getBytes(nrfByteAddress, 6);
    Mirf.setTADDR(nrfByteAddress);

    // Prepare message to send
    String msgToSend = String(DOXEO_ADDR_MOTHER) + ';' + parseCommand(queue, ';', 1) + ";" + _sendId + ";" + parseCommand(queue, ';', 2);
    msgToSend.getBytes(_bufferToSend, 32);

    // prepare success message to be returned
    _successMsgExpected = parseCommand(queue, ';', 1) + ";" + String(DOXEO_ADDR_MOTHER) + ";" + _sendId + ";success";

    // increase message ID
    _sendId++;
    if (_sendId == 0) {
      _sendId++;
    }

    if (parseCommand(queue, ';', 0) == "nrf2") {
      _sendMaxNumber = 100; // take 4,5s
      _emergencySending = true;
    } else {
      _sendMaxNumber = 60;
      _emergencySending = false;
    }

    _remainingSend = _sendMaxNumber;
    _tryNumber = 5;
    _ackReceived = false;

    Serial.println("Sending " + msgToSend);
  }
}

void Nrf::sendProcess()
{
  if (_remainingSend == 0) {
    // nothing to do
  } else if (_remainingSend == 1) {
    Serial.println("error;the message " + String((char*) _bufferToSend) + " has not been received acknowledge!");
    Serial.println("nrf;" + parseCommand(String((char*) _bufferToSend), ';', 1) + ";no_acknowledge");
    _remainingSend = 0;
    _emergencySending = false;
    _successMsgExpected = "";
  } else if (_ackReceived) {
    if ((millis() - _lastSendTime) >= 1000) {
      _ackReceived = false;

      if (_tryNumber > 0) {
        _tryNumber--;
        _remainingSend = _sendMaxNumber;
        Serial.println("no success msg expected received! retry");
      } else {
        _remainingSend = 1;
        Serial.println("no success msg expected received!");
      }
    }
  } else if (_remainingSend > 1 && (millis() - _lastSendTime) >= 5) {
    // send msg
    Mirf.configRegister(EN_RXADDR, 0x03); // only pipe 0 and 1 can received for ACK
    Mirf.send(_bufferToSend);
    while (Mirf.isSending()); // take 40ms with 15x retry
    Mirf.configRegister(EN_RXADDR, 0x02); // only pipe 1 can received

    if (Mirf.sendWithSuccess == true) {
      Serial.println(String((char *)_bufferToSend) + " send (" + String(_sendMaxNumber - _remainingSend + 1) + "x)");
      _ackReceived = true;
    } else {
      _ackReceived = false;
    }

    _remainingSend--;
    _lastSendTime = millis();
  }
}

bool Nrf::emergencySending()
{
  return _emergencySending;
}

void Nrf::interruptHandler()
{
  _newMsgReceived = true;
}

String Nrf::parseCommand(String data, char separator, int index)
{
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

