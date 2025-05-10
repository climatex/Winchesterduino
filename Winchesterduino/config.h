// Winchesterduino (c) 2025 J. Bogin, http://boginjr.com
// Build configuration

#pragma once

// WDC defines
#define WD10C20B               0         // set to 1 to disable RLL option and to keep RLL/MFM output floating, if WD10C20B separator is used
                                         // (line RLLMFM treated as OUT to separator chip pin 17, this is GND in a WD10C20B!)
#define WDC_TIMEOUT_HALT       1         // stop execution if the controller is present (RAM check ok), but WCLOCK is missing
// seeking 
#define SEEK_PULSE_US          3         // seek pulse length, microseconds
#define SLOWSEEK_SRT_MS        4         // head step rate time in ms to wait before next step (slow seek mode)
#define FASTSEEK_SRT_US        10        // microseconds to wait before next step (buffered seek)

// UI defines
#define MAX_PROGMEM_STRING_LEN 60        // maximum number of characters for each string in PROGMEM, MAX+1 size of buffer for pgm_read_ptr()
#define MAX_CHARS              100       // ui->print() buffer size
#define MAX_PROMPT_LEN         100       // ui->prompt() buffer size

// filesystem defines
#define MAX_PATH               100       // max path, MAX_PATH+1 size of path buffer

// just in case
#undef BYTE
#undef WORD
#undef DWORD
#define BYTE uint8_t
#define WORD uint16_t
#define DWORD uint32_t

// internal includes and used libraries
#include <Arduino.h>
#include <avr/sfr_defs.h>
#include <string.h>
#include <EEPROM.h>
#include "src/XModem/XModem.h"
#include "src/FatFs/ff.h"

// our common includes
#include "progmem.h"
#include "ui.h"
#include "wd42c22.h"
#include "image.h"
#include "eeprom.h"
#include "main.h"
#include "dos.h"

// public globals
extern Ui*      ui;
extern WD42C22* wdc;