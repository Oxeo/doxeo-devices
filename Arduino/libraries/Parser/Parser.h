#ifndef Parser_h
#define Parser_h

#include "Arduino.h"

class Parser
{   
  public:
    Parser(const char separator);
    void parse(const char* data);
    char* get(const byte index);
    int getInt(const byte index);
    bool isEqual(const byte index, const char* msg);
    
  private:
    char _buffer[26]; // maximum MySensors payload size is 25 bytes
    char* _indexList[5];
    char _separator;
};

#endif

