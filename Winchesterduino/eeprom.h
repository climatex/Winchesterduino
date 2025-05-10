// Winchesterduino (c) 2025 J. Bogin, http://boginjr.com
// EEPROM helper functions to load and save drive configuration

// there's already one EEPROM.h of the Arduino framework
#ifndef _WINCHESTERDUINO_EEPROM_H_
#define _WINCHESTERDUINO_EEPROM_H_

bool eepromLoadConfiguration();
void eepromStoreConfiguration();
void eepromClearConfiguration();

#endif