#ifndef SIM900_H
#define SIM900_H

#include <Arduino.h>
#include <SoftwareSerial.h>

struct Sms {
  String msg;
  String numbers;
};

class Sim900 {
  public:
    Sim900(int rx, int tx);
    void init();
    void update();
    void sendAtCmd(String message);
    bool isError();
    String getError();
    bool isInfo();
    String getInfo();

    void sendSms(String numbers, String message);
    bool newSms();
    bool newData();
    char* getData();
    
  protected:
    void parseSms();
    bool readSIM900();
    char strContains(char *str, char *sfind);

    int state = 0;
    unsigned long stateTimer = 0;
    Sms smsToSend;
    char buffer[100];
    String error;
    String info;
    bool newSmsReceived;
    bool newDataReceived;
    SoftwareSerial *serial = NULL;
};

#endif /* SIM900_H */


