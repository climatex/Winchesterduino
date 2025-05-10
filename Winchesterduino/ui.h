// Winchesterduino (c) 2025 J. Bogin, http://boginjr.com
// User interface... adapted from MegaFDC, just no physical displays here :-)

#pragma once
#include "config.h"

// readkey with wait or without
#define READKEY_CHECK_WAIT if (withWait) continue; else return 0;

class Ui
{
public:
  static Ui* get()
  {
    static Ui ui;
    return &ui;
  }
  
  void reset();
  
  void print(const BYTE* str, ...);
  BYTE readKey(const BYTE* allowedKeys = NULL, bool withWait = true);
  const BYTE* prompt(BYTE maximumPromptLen = 0, const BYTE* allowedKeys = NULL, bool escReturnsNull = false);
    
  const BYTE* getPromptBuffer() { return (const BYTE*)&m_promptBuffer[0]; }
  BYTE* getPrintBuffer() { return &m_printBuffer[0]; }
  void setPrintLength(WORD length) { m_printLength = length; }   
  void setPrintDisabled(bool disable) { m_printDisabled = disable; }
  void fatalError(BYTE progmemStrIndex);
  
private:  
  Ui(); 

  BYTE m_printBuffer[MAX_CHARS + 1];
  BYTE m_promptBuffer[MAX_PROMPT_LEN + 1];
  WORD m_printLength;
  
  bool m_printDisabled;
};