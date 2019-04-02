#include "Parser.h"

Parser::Parser(const char separator) {
    _separator = separator;
    
    // add first index
    _indexList[0] = _buffer;
    
    // end with null character
    _buffer[25] = 0;
}

void Parser::parse(const char* data)
{
    byte indexCount = 1;
    
    // clear index list
    for (byte i=1; i < 5; ++i) {
        _indexList[i] = NULL;
    }
    
    // copy and replace separator by null character
    for (byte i=0; i < strlen(data) + 1 && i + 1 < sizeof(_buffer); ++i) {
        if (data[i] != _separator) {
            _buffer[i] = data[i];
        } else {
            _buffer[i] = 0;
            
            if (indexCount < 5 && i + 2 < sizeof(_buffer)) {
                _indexList[indexCount] = _buffer + i + 1;
                indexCount++;
            }
        }
    }
}

char* Parser::get(const byte index)
{
  if (index < 5) {
    return _indexList[index];
  } else {
    return NULL;
  }
}

int Parser::getInt(const byte index)
{
  if (index < 5 && _indexList[index] != NULL) {
    return atoi(_indexList[index]);
  } else {
    return 0;
  }
}

bool Parser::isEqual(const byte index, const char* msg)
{
    if (index < 5 && _indexList[index] != NULL) {
        return strcmp(_indexList[index], msg) == 0;
    } else {
        return false;
    }
}

