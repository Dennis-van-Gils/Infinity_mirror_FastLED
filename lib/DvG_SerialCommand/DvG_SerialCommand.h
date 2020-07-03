// Dennis van Gils, 03-09-2016
// OUTDATED AND STRIPPED DOWN: DO NOT USE FOR OTHER PROJECTS

#ifndef H_DvG_SerialCommand
#define H_DvG_SerialCommand

#include <Arduino.h>

#define LF 10       // ASCII line feed
#define CR 13       // ASCII carriage return
#define STR_LEN 16  // Maximum length of the char array for incoming serial
                    // commands. Includes the '\0' termination character.

class DvG_SerialCommand {
 public:
  DvG_SerialCommand(Uart& mySerial);
  bool   available();
  char*  getCmd();

 private:
  Uart&    _port;             // HardwareSerial reference
  char     _strIn[STR_LEN];   // Incoming serial command string
  bool     _fTerminatedStrIn; // Incoming serial command is/got terminated?
  uint8_t  _iPos;             // Index within _strIn to insert new char
};

#endif
