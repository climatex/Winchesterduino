// Winchesterduino (c) 2025 J. Bogin, http://boginjr.com
// Entry point

#include "config.h"

// user interface
Ui* ui = NULL;

// Winchester disk controller
WD42C22* wdc = NULL;

void setup()
{ 
  ui = Ui::get();
  wdc = WD42C22::get();
  
  InitializeDisk();
}

void loop()
{ 
  MainLoop();
}