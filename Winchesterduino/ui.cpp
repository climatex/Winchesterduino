// Winchesterduino (c) 2025 J. Bogin, http://boginjr.com
// User interface... adapted from MegaFDC, just no physical displays here :-)

#include "config.h"

// reset by null pointer function call
void (*resetBoard)() = NULL;

// singleton
Ui::Ui()
{
  m_printDisabled = false;
  m_printLength = 0;

  // using a conservative value of 115 200 bps here (about 8K/s during XMODEM transfers)
  Serial.begin(115200);
  
  // values above this baud rate need to be conformant to the table at https://wormfood.net/avrbaudcalc.php
  // (fOSC = 16 MHz; U2xn = 1; while at error < 2 %)
  //
  // even though the CH340 on my Mega2560 board does not natively support a 500000bps rate,
  // (https://www.insidegadgets.com/wp-content/uploads/2016/12/ch340g-datasheet.pdf),
  // it is tested out to be working, and still "safe" enough for data transfers (~16K/s XMODEM-1K)
  // - although this requires a terminal app (such as TeraTerm) not "tied" to classic baud rates.
  // >=1 Mbps transfers would need a redesign of the XMODEM callback functions, or saving to an SD card, etc.
}

// reset board
void Ui::reset()
{
  // clear terminal, if enabled
  print("");
   
  // software reset by null pointer function call
  resetBoard();
}

// prints a null terminated string, supporting printf variadics (str != getPrintBuffer())
// or a string of custom m_printLength, with 0s and invalid characters skipped (str == getPrintBuffer())
void Ui::print(const BYTE* str, ...)
{
  // null pointer, or print has been disabled during XMODEM transfers - do not print anything
  if (!str || m_printDisabled)
  {
    return;
  }
     
  // if the input buffer is not the same as internal, do variadic expansion normally
  if (str != m_printBuffer)
  {
    va_list args;
    va_start(args, str);
    vsnprintf(m_printBuffer, sizeof(m_printBuffer), str, args);
    va_end(args);
  }
    
  // autoset print length, or if using custom, check bounds
  if (!m_printLength)
  {
    m_printLength = strlen(m_printBuffer);
  }  
  else if (m_printLength > MAX_CHARS)
  {
    m_printLength = MAX_CHARS;
  }   

  // print called with empty string ?
  if (!m_printLength)
  {
    Serial.print((const char*)Progmem::getString(Progmem::uiNewLine));
  }
  
  else
  {
    Serial.write(m_printBuffer, m_printLength);
    m_printLength = 0; // reset print length  
  }
}

BYTE Ui::readKey(const BYTE* allowedKeys, bool withWait)
{ 
  // check for allowed keys; null pointer means all (supported) are allowed
  bool checkAllowed = (allowedKeys != NULL);
  
  while(true)
  {
    // read keys through serial if UI not enabled
    int read = Serial.read();      
    if (read == -1)
    {
      READKEY_CHECK_WAIT;
    }
    
    // filter out invalid characters
    else if (read == 0x7f)
    {
      read = 0x08; // treat DEL as backspace if it came thru serial
    }
    else if (read > 0x7e)
    {  
      READKEY_CHECK_WAIT;
    }
    // allow CR (ENTER), Escape, and backspace
    else if ((read < 0x20) && (read != 0x0D) && (read != 0x1B) && (read != 0x08))
    { 
      READKEY_CHECK_WAIT;
    }
    
    BYTE chr = (BYTE)read;
    
    // no check for certain keys? return here
    if (!checkAllowed)
    {
      return chr;
    }
    
    // go thru the allowed characters string, re-read keyboard if key not in there
    for (WORD index = 0; index < strlen(allowedKeys); index++)
    {
      if (toupper(chr) == toupper(allowedKeys[index]))
      {
        return chr;
      }
    }
    
    READKEY_CHECK_WAIT;
  }
}

// prompt for string with a maximum length if set; allowed keys (if not null) shall contain at least \r\b
const BYTE* Ui::prompt(BYTE maximumPromptLen, const BYTE* allowedKeys, bool escReturnsNull)
{ 
  // buffer overflow check
  if (!maximumPromptLen || (maximumPromptLen > (sizeof(m_promptBuffer)-1)))
  {
    maximumPromptLen = sizeof(m_promptBuffer)-1;
  }
    
  // prompt for a command
  BYTE index = 0;
  memset(m_promptBuffer, 0, sizeof(m_promptBuffer));
  
  while(true)
  {
    BYTE chr = readKey(allowedKeys);
       
    // ENTER or CR on terminal: confirm prompt
    if (chr == '\r')
    {
      break;
    }
      
    // ESC, if allowed
    else if (chr == '\e')
    {
      // return as if prompt canceled
      if (escReturnsNull)
      {
        return NULL;
      }
      
      // default: cancel out current command and make a newline (like in DOS)      
      while (index > 0)
      {
        m_promptBuffer[--index] = 0;
      }
            
      // echo backslash with a newline
      print("\\\r\n");      
      continue;
    }
    
    // backspace - shorten the string by one
    else if (chr == '\b')
    {
      if (index > 0)
      {
        // make sure the backspace is destructive on a serial terminal
        print("\b \b");        
        m_promptBuffer[--index] = 0;
      }
      
      continue;
    }
    
    // any other key and the buffer is full?
    else if (index == maximumPromptLen)
    {
      continue;
    }
    
    // echo and store prompt buffer
    print("%c", chr);   
    m_promptBuffer[index++] = chr;    
  }
  
  return (const BYTE*)&m_promptBuffer;
}

// print an error that doesn't continue (in a XMODEM callback, this function does nothing)
void Ui::fatalError(BYTE progmemStrIndex)
{
  wdc->selectDrive(false); // turn the LED off
  
  if (!IsSerialTransfer())
  {
    print(Progmem::getString(Progmem::uiFatalError));
    print(Progmem::getString(progmemStrIndex));
    print(Progmem::getString(Progmem::uiSystemHalted));
    
    for (;;) {}
  }  
}