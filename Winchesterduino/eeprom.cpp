// Winchesterduino (c) 2025 J. Bogin, http://boginjr.com
// EEPROM helper functions to load and save drive configuration

#include "config.h"

// initialized EEPROM data structure
// offset (dec.), length in bytes, description:

// +00, 01, checksum byte
// +01, 01, number of drives configured (one)
// +02, 14, 1st drive configuration
// +16 to 4K: 0s

// simple 8-bit checksum
BYTE eepromComputeChecksum()
{
  BYTE checksum = 0;
  for (WORD index = 0; index < EEPROM.length(); index++)
  {
    checksum += (BYTE)EEPROM.read(index);
  }
  
  return checksum;
}

// overwrite checksum byte and set drive not configured
void eepromClearConfiguration()
{
  EEPROM.update(0, 0xFF);
  EEPROM.update(1, 0);
}

void eepromStoreConfiguration()
{
  // update number of drives
  EEPROM.update(0, 0);
  EEPROM.update(1, 1);

  // data
  WORD eepromOffset = 2;
  const BYTE* params = (const BYTE*)wdc->getParams();
  
  for (BYTE bufIndex = 0; bufIndex < sizeof(WD42C22::DiskDriveParams); bufIndex++)
  {
    EEPROM.update(eepromOffset++, params[bufIndex]);
  }
    
  // the rest is zeros
  while (eepromOffset < EEPROM.length())
  {
    EEPROM.update(eepromOffset++, 0);
  }
    
  // fill in the first byte so that the EEPROM checksum equals 0
  BYTE checksum = eepromComputeChecksum();
  if (checksum != 0)
  {
    checksum = 0x100 - checksum;
  }
  EEPROM.update(0, checksum);
}

// load configuration
bool eepromLoadConfiguration()
{
  // check drive count validity
  const BYTE driveCount = EEPROM.read(1);  
  if (driveCount != 1)
  {
    return false;
  }
   
  // checksum must be zero
  if (eepromComputeChecksum() != 0)
  {
    return false;
  }
   
  // proceed
  BYTE eepromOffset = 2; // skip checksum byte and 1 drive cfg'd
  BYTE* params = (BYTE*)wdc->getParams();
    
  for (BYTE bufIndex = 0; bufIndex < sizeof(WD42C22::DiskDriveParams); bufIndex++)
  {
    params[bufIndex] = EEPROM.read(eepromOffset++);
  }
  
  // do a quick verify of what has been loaded
  WD42C22::DiskDriveParams* check = wdc->getParams();
  if ((check->Cylinders == 0) || (check->Cylinders > 2048) ||
      (check->Heads == 0) || (check->Heads > 16) || (check->DataVerifyMode > 2) ||
      (check->WritePrecompStartCyl > 2047) || (check->RWCStartCyl > 2047) ||
      (check->LandingZone > 2047) || 
      (check->PartialImageStartCyl > 2047) || (check->PartialImageEndCyl > 2047))
  {
    eepromClearConfiguration();
    memset(check, 0, sizeof(WD42C22::DiskDriveParams));
    return false;
  }
  
  // in case this was stored and recompiled with a different separator
#if defined(WD10C20B) && (WD10C20B == 1)
  check->UseRLL = false;
#endif

  return true;
}