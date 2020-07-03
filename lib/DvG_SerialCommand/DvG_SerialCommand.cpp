// Dennis van Gils, 03-09-2016

// OUTDATED AND STRIPPED DOWN: DO NOT USE FOR OTHER PROJECTS

#include "DvG_SerialCommand.h"

DvG_SerialCommand::DvG_SerialCommand(Uart& mySerial) :
_port(mySerial)   // Need to initialise references before body
{
  _strIn[0] = '\0';
  _fTerminatedStrIn = false;
  _iPos = 0;
}

bool DvG_SerialCommand::available() {
  // Polls the hardware serial buffer for characters and appends these into one
  // command string. The appending stops as soon as a line feed (ASCII 10)
  // character is found or when the maximum length is reached in which case the
  // serial command is forcefully terminated. This function returns true when
  // the serial command is/got terminated, else returns false.
  // Note: carriage return (ASCII 13) characters are ignored.
  char c;

  // Poll serial buffer
  if (_port.available()) {
    _fTerminatedStrIn = false;
    while (_port.available()) {
      c = _port.peek();
      if (c == CR) {
        // Ignore ASCII 13 (carriage return)
        _port.read();             // Remove char from serial buffer
      } else if (c == LF) {
        // Found the proper termination character ASCII 10 (line feed)
        _port.read();             // Remove char from serial buffer
        _strIn[_iPos] = '\0';     // Terminate string
        _fTerminatedStrIn = true;
        break;
      } else if (_iPos < STR_LEN - 1) {
        // Maximum length of incoming serial command is not yet reached. Append
        // characters to string.
        _port.read();             // Remove char from serial buffer
        _strIn[_iPos] = c;
        _iPos++;
      } else {
        // Maximum length of incoming serial command is reached. Forcefully
        // terminate string now. Leave the char in the serial buffer.
        _strIn[_iPos] = '\0';     // Terminate string
        _fTerminatedStrIn = true;
        break;
      }
    }
  }
  return _fTerminatedStrIn;
}

char* DvG_SerialCommand::getCmd() {
  // Returns the incoming serial command only when it was properly terminated,
  // otherwise returns an empty string.
  if (_fTerminatedStrIn) {
    _fTerminatedStrIn = false;    // Reset incoming serial command string
    _iPos = 0;                    // Reset incoming serial command string
    return (char*) _strIn;
  } else {
    return '\0';
  }
}
