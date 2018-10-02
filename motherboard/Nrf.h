#ifndef Nrf_h
#define Nrf_h

#include "Arduino.h"
#include <Mirf.h>
#include <QueueList.h> // https://playground.arduino.cc/Code/QueueList
#include <SPI.h>      // Pour la communication via le port SPI
#include <Mirf.h>     // Pour la gestion de la communication
#include <nRF24L01.h> // Pour les définitions des registres du nRF24L01
#include <MirfHardwareSpiDriver.h> // Pour la communication SPI (ne cherchez pas à comprendre)

#include <DoxeoConfig.h>

class Nrf
{
  public:
    Nrf(int pinInterrupt);
    void init();
    void sendMessage(String message);
    void update();
    bool emergencySending();
    static String parseCommand(String data, char separator, int index);
    
  private:
    int _pinInterrupt;
    QueueList <String> _sendQueue;
    byte _bufferToSend[32];
    unsigned long _sendId = 1;
    int _remainingSend = 0;
    int _sendMaxNumber = 0;
    unsigned long _lastSendTime = 0;
    String _successMsgExpected = "";
    static bool _newMsgReceived;
    bool _emergencySending = false;
    char _tryNumber;
    bool _ackReceived;

    void sendProcess();
    void unstackMessageToSend();
    void checkNewMessage();
    static void interruptHandler();
};

#endif
