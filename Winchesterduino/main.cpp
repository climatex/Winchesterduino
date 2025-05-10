// Winchesterduino (c) 2025 J. Bogin, http://boginjr.com
// Main functions

#include "config.h"

bool minimalMode = false; // WDC not working or not detected, only allow seeking

// forward decl's
void SetupParameters();
void MinimalMode();

// main menu commands
void CommandAnalyze();
void CommandHexdump();
void CommandFormat();
void CommandScan();
void CommandShowParams();
void CommandPark();
// declared in image.h
//void CommandReadImage();
//void CommandWriteImage();
// declared in dos.h
//void CommandDos();

void InitializeDisk()
{
  // gibberish over serial initialization upon reset, sometimes
  ui->print(Progmem::getString(Progmem::uiVT100ClearScreen));
  ui->print(Progmem::getString(Progmem::uiSplash));
  ui->print(Progmem::getString(Progmem::uiBuild));
  ui->print(Progmem::getString(Progmem::uiNewLine2x));
   
  // test WDC 
  ui->print(Progmem::getString(Progmem::uiTestingWDC));
  minimalMode = !wdc->testBoard();
  ui->print(" ");
  ui->print(Progmem::getString(minimalMode ? Progmem::uiFAIL : Progmem::uiOK));
  ui->print(Progmem::getString(Progmem::uiNewLine));
  
  // select the drive and wait until it is ready
  wdc->selectDrive();
  ui->print(Progmem::getString(Progmem::uiWaitingReady));
  while (!wdc->isDriveReady()) {}
  ui->print(" ");
  ui->print(Progmem::getString(Progmem::uiOK));
  ui->print(Progmem::getString(Progmem::uiNewLine));
  
  // detect track 0, this throws an internal error if it fails
  ui->print(Progmem::getString(Progmem::uiSeekingToCyl0));
  wdc->recalibrate();
  ui->print(" ");
  ui->print(Progmem::getString(Progmem::uiOK));
  ui->print(Progmem::getString(Progmem::uiNewLine));
  
  // we can continue, setup and apply new parameters
  wdc->selectDrive(false); // not needed now
  SetupParameters();
  if (!minimalMode)
  {
    wdc->applyParams();
  }
}

void MainLoop()
{ 
  if (minimalMode)
  {
    MinimalMode(); // just seek to given cylinder and head
  }
   
  // main menu
  char allowedKeys[10] = {0};
  strcat(allowedKeys, "AHFMRWSI");
  if (wdc->getParams()->UseLandingZone)
  {
    strcat(allowedKeys, "P"); // offer park command
  }
  
  for (;;)
  {
    // make sure we're at cylinder 0 after any operation
    ui->print("");    
    wdc->seekDrive(0, 0);
    wdc->selectDrive(false); // not needed during command specifications
    
    ui->print(Progmem::getString(Progmem::optionAnalyze));
    ui->print(Progmem::getString(Progmem::optionHexdump));
    ui->print(Progmem::getString(Progmem::optionFormat));
    ui->print(Progmem::getString(Progmem::optionScan));
    ui->print(Progmem::getString(Progmem::optionReadImage));
    ui->print(Progmem::getString(Progmem::optionWriteImage));
    ui->print(Progmem::getString(Progmem::optionShowParams));
    ui->print(Progmem::getString(Progmem::optionDos));
    if (wdc->getParams()->UseLandingZone)
    {
      ui->print(Progmem::getString(Progmem::optionPark));
    }
    ui->print(Progmem::getString(Progmem::uiNewLine));
    ui->print(Progmem::getString(Progmem::uiChooseOption));
    
    BYTE mainmenu = toupper(ui->readKey(allowedKeys));
    ui->print(Progmem::getString(Progmem::uiEchoKey), mainmenu);
    
    switch(mainmenu)
    {
    case 'A':
      CommandAnalyze();
      break;
    case 'H':
      CommandHexdump();
      break;
    case 'F':
      CommandFormat();
      break;
    case 'M':
      CommandScan();
      break;
    case 'R':
      CommandReadImage();
      break;
    case 'W':
      CommandWriteImage();
      break;
    case 'S':
      CommandShowParams();
      break;
    case 'I':
      CommandDos();
      break;
    case 'P':
      CommandPark();
      break;
    }
  }
}

void SetupParameters()
{
  BYTE key = 0;
  ui->print(Progmem::getString(Progmem::uiNewLine));
  
  // no controller; only ask for seek type
  if (minimalMode)
  {
    ui->print(Progmem::getString(Progmem::uiSetupParams));
    ui->print(Progmem::getString(Progmem::uiSetupAskSeek));
    key = toupper(ui->readKey("BS"));
    wdc->getParams()->SlowSeek = (key == 'S');
    ui->print(Progmem::getString(Progmem::uiEchoKey), key);
    
    // for seekDrive() not to check max. cylinder and head extents; use full
    wdc->getParams()->Cylinders = 2048;
    wdc->getParams()->Heads = 16;
    
    // and that's all
    return;    
  }
  
  const bool savedSettings = eepromLoadConfiguration();
  if (savedSettings)
  {
    // print them out
    ui->print(Progmem::getString(Progmem::uiSetupSaved));
    CommandShowParams();
    
    // give a timeout from keyboard to respecify these
    bool autoLoad = true;
    ui->print(Progmem::getString(Progmem::uiSetupSavedLoad));
    ui->print(Progmem::getString(Progmem::uiAbort));
    
    DWORD timeBefore = millis();
    while (millis() - timeBefore < 2500)
    {
      key = ui->readKey("\e", false); // check if Esc pressed and return immediately
      if (key == '\e')
      {
        autoLoad = false;
        break;
      }
    }
    ui->print(Progmem::getString(Progmem::uiDeleteLine));
    
    if (autoLoad)
    {
      // we're done
      return;
    }
    
    // aborted; before respecifying, give option to remove them from EEPROM
    ui->print(Progmem::getString(Progmem::uiSetupAskRemove));
    key = toupper(ui->readKey("YN"));
    ui->print(Progmem::getString(Progmem::uiEchoKey), key);
    if (key == 'Y')
    {
      eepromClearConfiguration();
    }    
    
    memset(wdc->getParams(), 0, sizeof(WD42C22::DiskDriveParams)); // clear all just to be safe
    ui->print(Progmem::getString(Progmem::uiNewLine));
  }
  
  // (re-)specify
  ui->print(Progmem::getString(Progmem::uiSetupParams));
  
#if defined(WD10C20B) && (WD10C20B == 1)
  wdc->getParams()->UseRLL = false; // on WD10C20B, always MFM
#else  
  ui->print(Progmem::getString(Progmem::uiSetupDataMode));
  key = toupper(ui->readKey("MR"));
  wdc->getParams()->UseRLL = (key == 'R');
  ui->print(Progmem::getString(Progmem::uiEchoKey), key);
#endif

  // data verify mode (in RLL always 56bit ECC)
  if (wdc->getParams()->UseRLL)
  {
    wdc->getParams()->DataVerifyMode = MODE_ECC_56BIT;
  }
  else
  {
    ui->print(Progmem::getString(Progmem::uiSetupDataVerify));
    key = toupper(ui->readKey("135"));
    ui->print(Progmem::getString(Progmem::uiEchoKey), key);
    switch(key)
    {
    case '1':
      wdc->getParams()->DataVerifyMode = MODE_CRC_16BIT;
      break;
    case '3':
      wdc->getParams()->DataVerifyMode = MODE_ECC_32BIT;
      break;
    case '5':
      wdc->getParams()->DataVerifyMode = MODE_ECC_56BIT;
      break;
    }
  }
  
  // cylinders 1-2048
  while(true)
  {
    ui->print(Progmem::getString(Progmem::uiSetupCylinders));
    WORD cylinders = (WORD)atoi(ui->prompt(4, Progmem::getString(Progmem::uiDecimalInput)));
    if ((cylinders > 0) && (cylinders <= 2048))
    {
      wdc->getParams()->Cylinders = cylinders;      
      ui->print(Progmem::getString(Progmem::uiNewLine));
      break;
    }
    
    ui->print(Progmem::getString(Progmem::uiDeleteLine));
  }
  
  // heads 1-16
  while(true)
  {
    ui->print(Progmem::getString(Progmem::uiSetupHeads));
    WORD heads = (WORD)atoi(ui->prompt(2, Progmem::getString(Progmem::uiDecimalInput)));
    if ((heads > 0) && (heads <= 16))
    {
      wdc->getParams()->Heads = heads;      
      ui->print(Progmem::getString(Progmem::uiNewLine));
      break;
    }
    
    ui->print(Progmem::getString(Progmem::uiDeleteLine));
  }
  
  // ask for reduced write current, if heads <= 8 (HDSEL3 used for RWC)
  if (wdc->getParams()->Heads <= 8)
  {
    ui->print(Progmem::getString(Progmem::uiSetupAskRWC));
    key = toupper(ui->readKey("YN"));
    wdc->getParams()->UseReduceWriteCurrent = (key == 'Y');
    ui->print(Progmem::getString(Progmem::uiEchoKey), key);
    
    if (key == 'Y')
    {
      // RWC starting cylinder 0-2047
      while(true)
      {
        ui->print(Progmem::getString(Progmem::uiSetupCylRWC));
        WORD startCyl = (WORD)atoi(ui->prompt(4, Progmem::getString(Progmem::uiDecimalInput)));
        if (startCyl < 2048)
        {
          wdc->getParams()->RWCStartCyl = startCyl;
          if (!startCyl && !strlen(ui->getPromptBuffer())) ui->print("0");
          ui->print(Progmem::getString(Progmem::uiNewLine));
          break;
        }
        
        ui->print(Progmem::getString(Progmem::uiDeleteLine));
      }
    }
  }
  
  // ask for reduced write precompensation
  ui->print(Progmem::getString(Progmem::uiSetupAskPrecomp));
  key = toupper(ui->readKey("YN"));
  wdc->getParams()->UseWritePrecomp = (key == 'Y');
  ui->print(Progmem::getString(Progmem::uiEchoKey), key);
  
  if (key == 'Y')
  {
    // precomp starting cylinder 0-2047
    while(true)
    {
      ui->print(Progmem::getString(Progmem::uiSetupCylPrecomp));
      WORD startCyl = (WORD)atoi(ui->prompt(4, Progmem::getString(Progmem::uiDecimalInput)));
      if (startCyl < 2048)
      {
        wdc->getParams()->WritePrecompStartCyl = startCyl;  
        if (!startCyl && !strlen(ui->getPromptBuffer())) ui->print("0");        
        ui->print(Progmem::getString(Progmem::uiNewLine));
        break;
      }
      
      ui->print(Progmem::getString(Progmem::uiDeleteLine));
    }
  }
  
  // ask if drive has landing zone
  ui->print(Progmem::getString(Progmem::uiSetupAskLZ));
  key = toupper(ui->readKey("YN"));
  wdc->getParams()->UseLandingZone = (key == 'Y');
  ui->print(Progmem::getString(Progmem::uiEchoKey), key);
  
  if (key == 'Y')
  {
    // LZ starting cylinder 0-2047
    while(true)
    {
      ui->print(Progmem::getString(Progmem::uiSetupCylLZ));
      WORD startCyl = (WORD)atoi(ui->prompt(4, Progmem::getString(Progmem::uiDecimalInput)));
      if (startCyl < 2048)
      {
        wdc->getParams()->LandingZone = startCyl;
        if (!startCyl && !strlen(ui->getPromptBuffer())) ui->print("0");
        ui->print(Progmem::getString(Progmem::uiNewLine));
        break;
      }
      
      ui->print(Progmem::getString(Progmem::uiDeleteLine));
    }
  }
 
  // seek slow or buffered 
  ui->print(Progmem::getString(Progmem::uiSetupAskSeek));
  key = toupper(ui->readKey("BS"));
  wdc->getParams()->SlowSeek = (key == 'S');
  ui->print(Progmem::getString(Progmem::uiEchoKey), key);
  
  // save settings?
  ui->print(Progmem::getString(Progmem::uiSetupAskSave));
  key = toupper(ui->readKey("YN"));
  ui->print(Progmem::getString(Progmem::uiEchoKey), key);
  if (key == 'Y')
  {
    ui->print(Progmem::getString(Progmem::uiOperationPending));
        
    eepromStoreConfiguration();
        
    ui->print(Progmem::getString(Progmem::uiOK));
    ui->print(Progmem::getString(Progmem::uiNewLine));
  }
}

void MinimalMode()
{
  ui->print("");
  ui->print(Progmem::getString(Progmem::uiMinimalMode1));
  ui->print(Progmem::getString(Progmem::uiMinimalMode2));
  ui->print(Progmem::getString(Progmem::uiMinimalMode3));
  ui->print(Progmem::getString(Progmem::uiNewLine));
  
  // in minimal mode, keep always selected, otherwise the read data signal would be zero 
  wdc->selectDrive();
  
  for (;;)
  {
    // manual seek to cylinder and head
    WORD cylToSeek = 0;
    BYTE headToSeek = 0;
    
    while(true)
    {
      ui->print(Progmem::getString(Progmem::uiMinimalModeSeek1));
      cylToSeek = (WORD)atoi(ui->prompt(4, Progmem::getString(Progmem::uiDecimalInput)));
      if (cylToSeek < 2048)
      {  
        if (!cylToSeek && !strlen(ui->getPromptBuffer())) ui->print("0");
        ui->print(Progmem::getString(Progmem::uiNewLine));
        break;
      }
      
      ui->print(Progmem::getString(Progmem::uiDeleteLine));
    }
    
    while(true)
    {
      ui->print(Progmem::getString(Progmem::uiMinimalModeSeek2));
      headToSeek = (WORD)atoi(ui->prompt(2, Progmem::getString(Progmem::uiDecimalInput)));
      if (headToSeek < 16)
      {  
        if (!headToSeek && !strlen(ui->getPromptBuffer())) ui->print("0");
        ui->print(Progmem::getString(Progmem::uiNewLine));
        break;
      }
      
      ui->print(Progmem::getString(Progmem::uiDeleteLine));
    }
    
    wdc->seekDrive(cylToSeek, headToSeek);
    ui->print(Progmem::getString(Progmem::uiMinimalModeSeek3));
  }
}

void CommandAnalyze()
{
  ui->print(Progmem::getString(Progmem::uiEscGoBack));
  
  // start and end cylinder
  WORD startCylinder = 0;
  if (wdc->getParams()->Cylinders > 1)
  {
    while(true)
    {
      ui->print(Progmem::getString(Progmem::uiChooseStartCyl), 0, wdc->getParams()->Cylinders-1);
      const BYTE* prompt = ui->prompt(4, Progmem::getString(Progmem::uiDecimalInputEsc), true);
      if (!prompt) // ESC key returns to main menu
      {
        ui->print(Progmem::getString(Progmem::uiNewLine));
        return;
      }
      startCylinder = (WORD)atoi(prompt);
      if (startCylinder < wdc->getParams()->Cylinders)
      {  
        if (!startCylinder && !strlen(ui->getPromptBuffer())) ui->print("0");
        ui->print(Progmem::getString(Progmem::uiNewLine));
        break;
      }
      
      ui->print(Progmem::getString(Progmem::uiDeleteLine));
    }
  }  
  
  // allow to customize end cylinder, do not ask for it if start is the last one
  WORD endCylinder = wdc->getParams()->Cylinders-1;
  if (startCylinder != endCylinder)
  {
    while(true)
    {
      ui->print(Progmem::getString(Progmem::uiChooseEndCyl), startCylinder, wdc->getParams()->Cylinders-1);
      const BYTE* prompt = ui->prompt(4, Progmem::getString(Progmem::uiDecimalInputEsc), true);
      if (!prompt)
      {
        ui->print(Progmem::getString(Progmem::uiNewLine));
        return;
      }
      endCylinder = (WORD)atoi(prompt);
      if ((endCylinder >= startCylinder) && (endCylinder < wdc->getParams()->Cylinders))
      {  
        if (!endCylinder && !strlen(ui->getPromptBuffer())) ui->print("0");
        ui->print(Progmem::getString(Progmem::uiNewLine));
        break;
      }
      
      ui->print(Progmem::getString(Progmem::uiDeleteLine));
    }  
  }
  
  // print sector numberings?  
  ui->print(Progmem::getString(Progmem::analyzePrintOrder));
  BYTE key = toupper(ui->readKey("YN\e"));
  if (key == '\e')
  {
    ui->print(Progmem::getString(Progmem::uiNewLine));
    return;
  }
  bool sectorNumberings = (key == 'Y');
  ui->print(Progmem::getString(Progmem::uiEchoKey), key);
  
  bool diskNotEmpty = false;
  
  // warnings
  bool headMismatch = false;
  bool cylinderMismatch = false;  
  bool variableSectorSize = false;  
  
  for (WORD cylinder = startCylinder; cylinder <= endCylinder; cylinder++)
  {

    // for all heads
    BYTE head = 0;
    while (head < wdc->getParams()->Heads)
    {   
      // execute scanID first, up to 5 attempts
      wdc->seekDrive(cylinder, head);

      BYTE sdh;
      BYTE attempts = 5;      
      while (attempts)
      {
        WORD dummy;
        BYTE dummy2;        
        wdc->scanID(dummy, dummy2, sdh);
        
        // first three (WDC timeout, drive not ready, writefault) abort the command
        if (wdc->getLastError())
        {
          if (wdc->getLastError() < 4)
          {
            ui->print(Progmem::getString(Progmem::uiNewLine));
            ui->print(Progmem::getString(wdc->getLastErrorMessage()));
            ui->print(Progmem::getString(Progmem::uiNewLine));
            return;        
          }
          
          attempts--;
        }
        else
        {
          break;
        }
      }
      
      // no single valid sector ID found
      if (!attempts)
      {
        ui->print(Progmem::getString(Progmem::uiCHInfo), cylinder, head);
        ui->print(Progmem::getString(Progmem::analyzeNoSectors));
        head++;
        continue;
      }   
      
      bool thisVariableSectorSize = false; // flag to show "variable bytes" in the following status message
      bool thisHeadMismatch = false;       // show "@" at the end of line
      bool thisCylinderMismatch = false;   // "*"
      
      WORD tableCount = 0;
      BYTE sectorsPerTrack = 0;
      DWORD* sectorsTable = CalculateSectorsPerTrack(sdh, sectorsPerTrack, tableCount,
                                                     thisHeadMismatch, thisCylinderMismatch, thisVariableSectorSize);
      if (!sectorsPerTrack)
      {
        ui->print(Progmem::getString(Progmem::uiCHInfo), cylinder, head);
        ui->print(Progmem::getString(Progmem::analyzeNoSectors));
        if (sectorsTable)
        {
          delete[] sectorsTable;
        }
        
        head++;
        continue;
      }
            
      // "global" flags to fire a warning in the end, when we're done with the read
      // "local" flags - indicate with a symbol character to the current line
      diskNotEmpty |= true;
      variableSectorSize |= thisVariableSectorSize; 
      headMismatch |= thisHeadMismatch;
      cylinderMismatch |= thisCylinderMismatch;
      
      const WORD sectorSize = wdc->getSectorSizeFromSDH(sdh);
      const BYTE interleave = CalculateInterleave(sectorsTable, tableCount, sectorsPerTrack);
      
      // print out info
      ui->print(Progmem::getString(Progmem::uiCHInfo), cylinder, head);
      if (!thisVariableSectorSize)
      {
        ui->print(Progmem::getString(Progmem::analyzeSectorInfo), sectorsPerTrack, sectorSize, interleave);  
      }
      else
      {
        ui->print(Progmem::getString(Progmem::analyzeSectorInfo2), sectorsPerTrack, interleave);
      }

      // append special characters to the line
      if (thisCylinderMismatch || thisHeadMismatch)
      {
        ui->print(" ");
        if (thisCylinderMismatch)      
        {
          ui->print("*");
        }
        if (thisHeadMismatch)
        {
          ui->print("@");
        }        
      }
      
      // sectors numbering
      if (sectorNumberings)
      {
        WORD startingSector = 0;
        WORD idx = 0;
        
        for (;;)
        {
          bool found = false;
          
          for (idx = 0; idx < tableCount; idx++)
          {                   
            if (sectorsTable[idx] == 0xFFFFFFFFUL)
            {
              continue;
            }
            
            // logical sector number matching?
            if ((BYTE)(sectorsTable[idx] >> 16) == startingSector)
            {
              found = true;
              break;
            }
          }
          
          if (found)
          {
            break;
          }
          
          startingSector++;  // starts from 1, 2 or whatever
          if (startingSector > tableCount) // cannot sync
          {
            break;
          }
        }
        
        if (startingSector > tableCount)
        {
          // don't write sector numbering
          delete[] sectorsTable;
          head++;
          continue;
        }

        ui->print(Progmem::getString(Progmem::analyzeSectorInfo3));
        
        WORD idx2 = 0;
        while (idx2 < sectorsPerTrack)
        {
          while ((idx < tableCount) && (idx2 < sectorsPerTrack))
          {
            ui->print("%u ", (BYTE)(sectorsTable[idx++] >> 16));
            idx2++;
          }
          
          // we still need to go from the beginning of the table?
          if (idx == tableCount)
          {
            idx = 0;
          }    
        }
      }      
                
      delete[] sectorsTable;
      head++;
    }    
  }

  ui->print(Progmem::getString(Progmem::uiNewLine2x));
  
  // print out analysis warnings
  if (cylinderMismatch || headMismatch || variableSectorSize)
  {
    ui->print(Progmem::getString(Progmem::analyzeWarning));
    
    if (cylinderMismatch)
    {
      ui->print(Progmem::getString(Progmem::analyzeCylMismatch));
    }
    if (headMismatch)
    {
      ui->print(Progmem::getString(Progmem::analyzeHdMismatch));
    }
    if (variableSectorSize)
    {
      ui->print(Progmem::getString(Progmem::analyzeVarSsize));
    }
    
    ui->print(Progmem::getString(Progmem::uiNewLine));
  }
  
  // normal status
  if (diskNotEmpty)
  {
    if (!cylinderMismatch && !headMismatch)
    {
      ui->print(Progmem::getString(Progmem::analyzeCylHdNormal));
    }
    if (!variableSectorSize)
    {
      ui->print(Progmem::getString(Progmem::analyzeConstSsize));
    }
  }  
}

void CommandHexdump()
{
  ui->print(Progmem::getString(Progmem::uiEscGoBack));
  
  // CHS
  WORD cylinder = 0;
  BYTE head = 0;
  BYTE sector = 0;
  
  if (wdc->getParams()->Cylinders > 1)
  {
    while(true)
    {
      ui->print(Progmem::getString(Progmem::uiChooseCylinder), 0, wdc->getParams()->Cylinders-1);
      const BYTE* prompt = ui->prompt(4, Progmem::getString(Progmem::uiDecimalInputEsc), true);
      if (!prompt)
      {
        ui->print(Progmem::getString(Progmem::uiNewLine));
        return;
      }
      cylinder = (WORD)atoi(prompt);
      if (cylinder < wdc->getParams()->Cylinders)
      {  
        if (!cylinder && !strlen(ui->getPromptBuffer())) ui->print("0");
        ui->print(Progmem::getString(Progmem::uiNewLine));
        break;
      }
      
      ui->print(Progmem::getString(Progmem::uiDeleteLine));
    }  
  }  
  
  if (wdc->getParams()->Heads > 1)
  {
    while(true)
    {
      ui->print(Progmem::getString(Progmem::uiChooseHead), 0, wdc->getParams()->Heads-1);
      const BYTE* prompt = ui->prompt(2, Progmem::getString(Progmem::uiDecimalInputEsc), true);
      if (!prompt)
      {
        ui->print(Progmem::getString(Progmem::uiNewLine));
        return;
      }
      head = (WORD)atoi(prompt);
      if (head < wdc->getParams()->Heads)
      {  
        if (!head && !strlen(ui->getPromptBuffer())) ui->print("0");
        ui->print(Progmem::getString(Progmem::uiNewLine));
        break;
      }
      
      ui->print(Progmem::getString(Progmem::uiDeleteLine));
    }
  }  
  
  while(true)
  {
    ui->print(Progmem::getString(Progmem::uiChooseSector), 0, 63);
    const BYTE* prompt = ui->prompt(2, Progmem::getString(Progmem::uiDecimalInputEsc), true);
    if (!prompt)
    {
      ui->print(Progmem::getString(Progmem::uiNewLine));
      return;
    }
    sector = (BYTE)atoi(prompt);
    if (sector <= 63)
    {  
      if (!sector && !strlen(ui->getPromptBuffer())) ui->print("0");
      ui->print(Progmem::getString(Progmem::uiNewLine));      
      break;
    }
    
    ui->print(Progmem::getString(Progmem::uiDeleteLine));
  }
  
  // sector size
  WORD sectorSizeBytes = 0;
  
  ui->print(Progmem::getString(Progmem::uiChooseSecSize));
  BYTE key = toupper(ui->readKey("125K\e"));
  switch(key)
  {
    case '1':
      sectorSizeBytes = 128;
      break;
    case '2':
      sectorSizeBytes = 256;
      break;
    case '5':
      sectorSizeBytes = 512;
      break;
    case 'K':
      sectorSizeBytes = 1024;
      break;
    case '\e':
      ui->print(Progmem::getString(Progmem::uiNewLine));
      return;
  }
  ui->print(Progmem::getString(Progmem::uiEchoKey), key);
  
  // long mode (get checksum bytes)
  bool longMode = false;  
  ui->print(Progmem::getString(Progmem::hexdumpLongMode));
  key = toupper(ui->readKey("YN\e"));
  if (key == '\e')
  {
    ui->print(Progmem::getString(Progmem::uiNewLine));
    return;
  }
  ui->print(Progmem::getString(Progmem::uiEchoKey), key);
  longMode = (key == 'N');  
  
  wdc->seekDrive(cylinder, head);
  wdc->readSector(sector, sectorSizeBytes, longMode);
  
  if (wdc->getLastError() && (wdc->getLastError() != WDC_DATAERROR))
  {
    // WDC timeout, drive not ready, writefault) abort the command
    if (wdc->getLastError() < 4)
    {  
      ui->print(Progmem::getString(Progmem::uiNewLine));
      ui->print(Progmem::getString(wdc->getLastErrorMessage()));
      ui->print(Progmem::getString(Progmem::uiNewLine));
      return;        
    }
    
    // prepend CHS information
    ui->print(Progmem::getString(Progmem::uiCHSInfo), cylinder, head, sector);
    ui->print(Progmem::getString(wdc->getLastErrorMessage()));
    ui->print(Progmem::getString(Progmem::uiNewLine));
  }
  
  // print the sector dump if no error, but also if there was a CRC/ECC error
  if (!wdc->getLastError() || (wdc->getLastError() == WDC_DATAERROR))
  {
    ui->print(Progmem::getString(Progmem::uiNewLine));
    ui->print(Progmem::getString(Progmem::hexdumpDump));
    
    wdc->sramBeginBufferAccess(false, 0);
    for (WORD index = 0; index < sectorSizeBytes; index++)
    {
      ui->print("%02X ", wdc->sramReadByteSequential());
    }
    ui->print(Progmem::getString(Progmem::uiNewLine));
    
    // print out checksum bytes
    if (longMode)
    {     
      BYTE polynomialStr = 0;
      BYTE checksumLength = 0;      
      
      switch(wdc->getParams()->DataVerifyMode)
      {
        case MODE_CRC_16BIT:
          checksumLength = 2;
          polynomialStr = Progmem::hexdumpPolynomial1;
          break;
        case MODE_ECC_56BIT:
          checksumLength = 7;
          polynomialStr = Progmem::hexdumpPolynomial3;
          break;
        case MODE_ECC_32BIT:
        default:
          checksumLength = 4;
          polynomialStr = Progmem::hexdumpPolynomial2;
          break;
      }
      
      ui->print(Progmem::getString(Progmem::uiNewLine));
      ui->print(Progmem::getString(Progmem::hexdumpChecksum), checksumLength*8, 
                                   (wdc->getParams()->DataVerifyMode == MODE_CRC_16BIT) ? "CRC" : "ECC");

      for (WORD index = 0; index < checksumLength; index++)
      {
        ui->print("%02X ", wdc->sramReadByteSequential());
      }
      ui->print(Progmem::getString(Progmem::uiNewLine));
      ui->print(Progmem::getString(polynomialStr));
      ui->print(Progmem::getString(Progmem::hexdumpChecksum2));
    }
    else
    {
      ui->print(Progmem::getString(Progmem::uiCHSInfo), cylinder, head, sector);
      if (!wdc->getLastError())
      {
        ui->print(Progmem::getString(Progmem::hexdumpOk));  
      }
      else
      {
        ui->print(Progmem::getString(wdc->getLastErrorMessage()));
      }
      ui->print(Progmem::getString(Progmem::uiNewLine));
    }
    
    wdc->sramFinishBufferAccess();
  }
}

void CommandFormat()
{
  ui->print(Progmem::getString(Progmem::formatWarning));
  ui->print(Progmem::getString(Progmem::uiEscGoBack));
  ui->print(Progmem::getString(Progmem::uiNewLine));
  
  // start and end cylinder
  WORD startCylinder = 0;
  
  if (wdc->getParams()->Cylinders > 1)
  {
    while(true)
    {
      ui->print(Progmem::getString(Progmem::uiChooseStartCyl), 0, wdc->getParams()->Cylinders-1);
      const BYTE* prompt = ui->prompt(4, Progmem::getString(Progmem::uiDecimalInputEsc), true);
      if (!prompt)
      {
        ui->print(Progmem::getString(Progmem::uiNewLine));
        return;
      }
      startCylinder = (WORD)atoi(prompt);
      if (startCylinder < wdc->getParams()->Cylinders)
      {  
        if (!startCylinder && !strlen(ui->getPromptBuffer())) ui->print("0");
        ui->print(Progmem::getString(Progmem::uiNewLine));
        break;
      }
      
      ui->print(Progmem::getString(Progmem::uiDeleteLine));
    }  
  }  
  
  WORD endCylinder = wdc->getParams()->Cylinders-1;
  if (startCylinder != endCylinder)
  {
    while(true)
    {
      ui->print(Progmem::getString(Progmem::uiChooseEndCyl), startCylinder, wdc->getParams()->Cylinders-1);
      const BYTE* prompt = ui->prompt(4, Progmem::getString(Progmem::uiDecimalInputEsc), true);
      if (!prompt)
      {
        ui->print(Progmem::getString(Progmem::uiNewLine));
        return;
      }
      endCylinder = (WORD)atoi(prompt);
      if ((endCylinder >= startCylinder) && (endCylinder < wdc->getParams()->Cylinders))
      {  
        if (!endCylinder && !strlen(ui->getPromptBuffer())) ui->print("0");
        ui->print(Progmem::getString(Progmem::uiNewLine));
        break;
      }
      
      ui->print(Progmem::getString(Progmem::uiDeleteLine));
    }  
  }
  
  // SPT
  BYTE sectorsPerTrack = 1;
  while(true)
  {
    ui->print(Progmem::getString(Progmem::formatSpt), 1, 63);
    const BYTE* prompt = ui->prompt(2, Progmem::getString(Progmem::uiDecimalInputEsc), true);
    if (!prompt)
    {
      ui->print(Progmem::getString(Progmem::uiNewLine));
      return;
    }
    sectorsPerTrack = (BYTE)atoi(prompt);
    if ((sectorsPerTrack > 0) && (sectorsPerTrack <= 63))
    {
      ui->print(Progmem::getString(Progmem::uiNewLine));
      break;
    }
    
    ui->print(Progmem::getString(Progmem::uiDeleteLine));
  }
  
  // sector size
  WORD sectorSizeBytes = 0;  
  ui->print(Progmem::getString(Progmem::uiChooseSecSize));
  BYTE key = toupper(ui->readKey("125K\e"));
  switch(key)
  {
    case '1':
      sectorSizeBytes = 128;
      break;
    case '2':
      sectorSizeBytes = 256;
      break;
    case '5':
      sectorSizeBytes = 512;
      break;
    case 'K':
      sectorSizeBytes = 1024;
      break;
    case '\e':
      ui->print(Progmem::getString(Progmem::uiNewLine));
      return;
  }
  ui->print(Progmem::getString(Progmem::uiEchoKey), key);
  
  // format interleave
  BYTE interleave = 1;
  while(true)
  {
    ui->print(Progmem::getString(Progmem::formatInterleave));
    const BYTE* prompt = ui->prompt(2, Progmem::getString(Progmem::uiDecimalInputEsc), true);
    if (!prompt)
    {
      ui->print(Progmem::getString(Progmem::uiNewLine));
      return;
    }
    interleave = (BYTE)atoi(prompt);
    if (interleave > 0)
    {
      ui->print(Progmem::getString(Progmem::uiNewLine));
      break;
    }
    
    ui->print(Progmem::getString(Progmem::uiDeleteLine));
  }
   
  // format with verify
  ui->print(Progmem::getString(Progmem::formatVerify));
  key = toupper(ui->readKey("YN\e"));
  if (key == '\e')
  {
    ui->print(Progmem::getString(Progmem::uiNewLine));
    return;
  }
  bool withVerify = (key == 'Y');
  ui->print(Progmem::getString(Progmem::uiEchoKey), key);
  
  // mark bad blocks
  bool formatBadBlocks = false;
  if (withVerify)
  {
    ui->print(Progmem::getString(Progmem::formatBadBlocks));
    key = toupper(ui->readKey("YN\e"));
    if (key == '\e')
    {
      ui->print(Progmem::getString(Progmem::uiNewLine));
      return;
    }
    formatBadBlocks = (key == 'Y');
    ui->print(Progmem::getString(Progmem::uiEchoKey), key);  
  }
  
  BYTE badBlocksCount = 0;
  BYTE* badBlocksTable = new BYTE[sectorsPerTrack];
  if (!badBlocksTable)
  {
    ui->fatalError(Progmem::uiFeMemory);
  }
  
  // format
  ui->print(Progmem::getString(Progmem::uiNewLine));  
  for (WORD cylinder = startCylinder; cylinder <= endCylinder; cylinder++)
  {
    // for all heads
    BYTE head = 0;
    while (head < wdc->getParams()->Heads)
    {   
      ui->print(Progmem::getString(Progmem::formatProgress), cylinder, head);
      
      wdc->prepareFormatInterleave(sectorsPerTrack, interleave);
      wdc->seekDrive(cylinder, head);      
      wdc->formatTrack(sectorsPerTrack, sectorSizeBytes);
      
      // formatTrack can only fail with WDC timeout, drive not ready or write fault
      if (wdc->getLastError())
      {
        ui->print(Progmem::getString(Progmem::uiNewLine2x));
        ui->print(Progmem::getString(wdc->getLastErrorMessage()));
        ui->print(Progmem::getString(Progmem::uiNewLine));
        delete[] badBlocksTable;
        return;
      }
      
      // with verify?
      if (withVerify)
      {
        badBlocksCount = 0;
        memset(badBlocksTable, 0, sectorsPerTrack);

        wdc->verifyTrack(sectorsPerTrack, sectorSizeBytes);
        if (wdc->getLastError())
        {
          // WDC timeout, drive not ready, writefault - abort the command
          if (wdc->getLastError() < 4)
          {  
            ui->print(Progmem::getString(Progmem::uiNewLine2x));
            ui->print(Progmem::getString(wdc->getLastErrorMessage()));
            ui->print(Progmem::getString(Progmem::uiNewLine));
            delete[] badBlocksTable;
            return;        
          }
          
          // fall back to single sector reads to determine which failed
          for (BYTE sector = 1; sector <= sectorsPerTrack; sector++)
          {
            wdc->readSector(sector, sectorSizeBytes); 
            
            if (wdc->getLastError())
            {
              if (wdc->getLastError() < 4)
              {  
                ui->print(Progmem::getString(Progmem::uiNewLine2x));
                ui->print(Progmem::getString(wdc->getLastErrorMessage()));
                ui->print(Progmem::getString(Progmem::uiNewLine));
                delete[] badBlocksTable;
                return;        
              }
              
              // mark sector as bad
              badBlocksTable[sector-1] = 1;
              badBlocksCount++;
              
              // prepend CHS information
              ui->print(Progmem::getString(Progmem::uiCHSInfo), cylinder, head, sector);
              ui->print(Progmem::getString(wdc->getLastErrorMessage()));
            }
          }
        }        

        // write bad blocks to disk?
        if (badBlocksCount && formatBadBlocks)
        {
          wdc->prepareFormatInterleave(sectorsPerTrack, interleave, badBlocksTable);
          wdc->formatTrack(sectorsPerTrack, sectorSizeBytes);
          
          if (wdc->getLastError())
          {
            ui->print(Progmem::getString(Progmem::uiNewLine2x));
            ui->print(Progmem::getString(wdc->getLastErrorMessage()));
            ui->print(Progmem::getString(Progmem::uiNewLine));
            delete[] badBlocksTable;
            return;
          }
        }
        
        if (badBlocksCount)
        {
          ui->print(Progmem::getString(Progmem::uiNewLine));
        }
      }

      head++;
    }    
  }
   
  delete[] badBlocksTable;
  ui->print(Progmem::getString(Progmem::formatComplete));  
}

void CommandScan()
{
  ui->print(Progmem::getString(Progmem::scanWarning1));
  ui->print(Progmem::getString(Progmem::scanWarning2));
  ui->print(Progmem::getString(Progmem::scanWarning3));
  ui->print(Progmem::getString(Progmem::uiEscGoBack));
  ui->print(Progmem::getString(Progmem::uiNewLine));
  
  // start and end cylinder
  WORD startCylinder = 0;
  if (wdc->getParams()->Cylinders > 1)
  {
    while(true)
    {
      ui->print(Progmem::getString(Progmem::uiChooseStartCyl), 0, wdc->getParams()->Cylinders-1);
      const BYTE* prompt = ui->prompt(4, Progmem::getString(Progmem::uiDecimalInputEsc), true);
      if (!prompt)
      {
        ui->print(Progmem::getString(Progmem::uiNewLine));
        return;
      }
      startCylinder = (WORD)atoi(prompt);
      if (startCylinder < wdc->getParams()->Cylinders)
      {  
        if (!startCylinder && !strlen(ui->getPromptBuffer())) ui->print("0");
        ui->print(Progmem::getString(Progmem::uiNewLine));
        break;
      }
      
      ui->print(Progmem::getString(Progmem::uiDeleteLine));
    }  
  }  
  
  WORD endCylinder = wdc->getParams()->Cylinders-1;
  if (startCylinder != endCylinder)
  {
    while(true)
    {
      ui->print(Progmem::getString(Progmem::uiChooseEndCyl), startCylinder, wdc->getParams()->Cylinders-1);
      const BYTE* prompt = ui->prompt(4, Progmem::getString(Progmem::uiDecimalInputEsc), true);
      if (!prompt)
      {
        ui->print(Progmem::getString(Progmem::uiNewLine));
        return;
      }
      endCylinder = (WORD)atoi(prompt);
      if ((endCylinder >= startCylinder) && (endCylinder < wdc->getParams()->Cylinders))
      {  
        if (!endCylinder && !strlen(ui->getPromptBuffer())) ui->print("0");
        ui->print(Progmem::getString(Progmem::uiNewLine));
        break;
      }
      
      ui->print(Progmem::getString(Progmem::uiDeleteLine));
    }  
  }
  
  bool marginalSectorsAsBad = true;
  if (wdc->getParams()->DataVerifyMode != MODE_CRC_16BIT)
  {
    ui->print(Progmem::getString(Progmem::scanMarginal));
    BYTE key = toupper(ui->readKey("YN\e"));
    if (key == '\e')
    {
      ui->print(Progmem::getString(Progmem::uiNewLine));
      return;
    }
    marginalSectorsAsBad = (key == 'Y');
    ui->print(Progmem::getString(Progmem::uiEchoKey), key);
  }
  
  DWORD existingBadBlocks = 0;
  DWORD unreadableTracks = 0;
  DWORD dataErrors = 0;
  ui->print(Progmem::getString(Progmem::uiNewLine));
   
  for (WORD cylinder = startCylinder; cylinder <= endCylinder; cylinder++)
  {
    ui->print(Progmem::getString(Progmem::scanProgress), cylinder);
    
    BYTE head = 0;
    while (head < wdc->getParams()->Heads)
    {   
      wdc->seekDrive(cylinder, head);

      BYTE sdh;
      BYTE attempts = 5;      
      while (attempts)
      {
        WORD dummy;
        BYTE dummy2;        
        wdc->scanID(dummy, dummy2, sdh);
        
        if (wdc->getLastError())
        {
          if (wdc->getLastError() < 4)
          {
            ui->print(Progmem::getString(Progmem::uiNewLine));
            ui->print(Progmem::getString(wdc->getLastErrorMessage()));
            ui->print(Progmem::getString(Progmem::uiNewLine));
            return;        
          }
          
          attempts--;
        }
        else
        {
          break;
        }
      }
      
      if (!attempts)
      {
        unreadableTracks++;
        head++;
        continue;
      }   
      
      bool dummy;      
      WORD tableCount = 0;
      BYTE sectorsPerTrack = 0;
      DWORD* sectorsTable = CalculateSectorsPerTrack(sdh, sectorsPerTrack, tableCount,
                                                     dummy, dummy, dummy);
      if (!sectorsPerTrack)
      {
        unreadableTracks++;
        if (sectorsTable)
        {
          delete[] sectorsTable;
        }
        
        head++;
        continue;
      }
      
      WORD trySectorSize = wdc->getSectorSizeFromSDH(sdh);
            
      // try whole track with uniform sector size
      wdc->verifyTrack(sectorsPerTrack, trySectorSize);      
      
      if (wdc->getLastError())
      {
        if (wdc->getLastError() < 4)
        {  
          ui->print(Progmem::getString(Progmem::uiNewLine2x));
          ui->print(Progmem::getString(wdc->getLastErrorMessage()));
          ui->print(Progmem::getString(Progmem::uiNewLine));
          delete[] sectorsTable;
          return;        
        }
        
        for (BYTE sector = 0; sector < sectorsPerTrack; sector++)
        {
          const BYTE sdh2 = (BYTE)(sectorsTable[sector] >> 24);
          trySectorSize = wdc->getSectorSizeFromSDH(sdh2);        
          
          const BYTE logicalSector = (BYTE)(sectorsTable[sector] >> 16);
          const WORD logicalCylinder = (WORD)sectorsTable[sector];
          const BYTE logicalHead = sdh2 & 0xF;        
        
          // single sector with variable sector size
          wdc->readSector(logicalSector, trySectorSize, false, &logicalCylinder, &logicalHead);
          BYTE error = wdc->getLastError();
          if ((error == WDC_CORRECTED) && !marginalSectorsAsBad)
          {
            error = WDC_OK; // cancel off error flag
          }
          
          if (error)
          {
            if (error < 4)
            {  
              ui->print(Progmem::getString(Progmem::uiNewLine2x));
              ui->print(Progmem::getString(wdc->getLastErrorMessage()));
              ui->print(Progmem::getString(Progmem::uiNewLine));
              delete[] sectorsTable;
              return;        
            }
            
            if ((error == WDC_DATAERROR) || (error == WDC_CORRECTED))
            {
              dataErrors++;
              
              // write ID as bad sector
              const BYTE interleave = CalculateInterleave(sectorsTable, tableCount, sectorsPerTrack);
              wdc->setBadSector(logicalSector, sectorsPerTrack, interleave, trySectorSize, &logicalCylinder, &logicalHead);
            }
            
            else
            {
              existingBadBlocks++;
            }
          }
        }
      }        
        
      delete[] sectorsTable;     
      head++;
    }    
  }

  ui->print(Progmem::getString(Progmem::uiNewLine2x));
  
  // results
  ui->print(Progmem::getString(Progmem::imgBadTracks), unreadableTracks);
  ui->print(Progmem::getString(Progmem::imgBadBlocksKnown), existingBadBlocks);
  ui->print(Progmem::getString(Progmem::imgDataErrorsConv), dataErrors);
  
}

void CommandShowParams()
{
  ui->print(Progmem::getString(Progmem::uiShowDataMode));
  ui->print(wdc->getParams()->UseRLL ? "RLL" : "MFM");
  
  ui->print(Progmem::getString(Progmem::uiShowVerifyMode));
  switch(wdc->getParams()->DataVerifyMode)
  {
  case MODE_CRC_16BIT:
    ui->print(Progmem::getString(Progmem::uiShowVerifyCRC));
    break;
  case MODE_ECC_32BIT:
    ui->print(Progmem::getString(Progmem::uiShowVerifyECC));
    break;
  case MODE_ECC_56BIT:
    ui->print(Progmem::getString(Progmem::uiShowVerifyECC56));
    break;
  }
  
  ui->print(Progmem::getString(Progmem::uiShowCylinders), wdc->getParams()->Cylinders);    
  ui->print(Progmem::getString(Progmem::uiShowHeads), wdc->getParams()->Heads);
  
  ui->print(Progmem::getString(Progmem::uiShowRWC));
  if (wdc->getParams()->UseReduceWriteCurrent)
  {
    ui->print(Progmem::getString(Progmem::uiEnabled));
    ui->print(Progmem::getString(Progmem::uiShowFromCyl), wdc->getParams()->RWCStartCyl);
  }
  else
  {
    ui->print(Progmem::getString(Progmem::uiDisabled));
  }
  
  ui->print(Progmem::getString(Progmem::uiShowPrecomp));
  if (wdc->getParams()->UseWritePrecomp)
  {
    ui->print(Progmem::getString(Progmem::uiEnabled));
    ui->print(Progmem::getString(Progmem::uiShowFromCyl), wdc->getParams()->WritePrecompStartCyl);
  }
  else
  {
    ui->print(Progmem::getString(Progmem::uiDisabled));
  }
  
  ui->print(Progmem::getString(Progmem::uiShowLZStatus));
  if (wdc->getParams()->UseLandingZone)
  {
    ui->print(Progmem::getString(Progmem::uiNo));
    ui->print(Progmem::getString(Progmem::uiShowLZ), wdc->getParams()->LandingZone); 
  }
  else
  {
    ui->print(Progmem::getString(Progmem::uiYes));
  }
  
  ui->print(Progmem::getString(Progmem::uiShowSeekMode));
  ui->print(Progmem::getString(wdc->getParams()->SlowSeek ? Progmem::uiShowSeekSlow : Progmem::uiShowSeekFast));
}

void CommandPark()
{
  ui->print(Progmem::getString(Progmem::uiNewLine));
  ui->print(Progmem::getString(Progmem::uiOperationPending));
  
  // temporarily override the seek check and park to the landing zone
  const WORD cyls = wdc->getParams()->Cylinders;
  wdc->getParams()->Cylinders = 2048;
  wdc->seekDrive(wdc->getParams()->LandingZone, 0);
  wdc->getParams()->Cylinders = cyls;
  wdc->selectDrive(false);
  
  ui->print(Progmem::getString(Progmem::parkSuccess), wdc->getParams()->LandingZone);
  ui->print(Progmem::getString(Progmem::parkPowerdownSafe));
  ui->print(Progmem::getString(Progmem::parkContinue));
  ui->readKey(NULL);

  // use the slow recalibrate command to seek back to 0, as the landing zone might be sometimes outside the cylinders count
  ui->print(Progmem::getString(Progmem::parkRecalibrating));
  wdc->recalibrate();
  ui->print(Progmem::getString(Progmem::uiDeleteLine));
}

DWORD* CalculateSectorsPerTrack(BYTE sdh, // input
                                BYTE& sectorsPerTrack, WORD& tableCount, // outputs
                                bool& headMismatch, bool& cylinderMismatch, bool& variableSectorSize) // ditto
{
  // returns maximum sectors per track observed, and a table of sectors of count tableCount
  // also sets output warning flags if non standard stuff were found
  // inside the table, each sector entry takes 4 bytes:
  // bits 31-24: SDH byte (contains bits: bad block, sector size and logical head number)
  //      23-16: logical sector number, ordered sequentially or with interleave
  //      15-8:  logical cylinder number, MSB
  //       7-0:  logical cylinder number, LSB
  // "logical" cylinder, head and sector numbers do not always correspond with physical CHS  
  
  sectorsPerTrack = 0;
  headMismatch = false;
  cylinderMismatch = false;  
  variableSectorSize = false;
  
  // up to 5 attempts at getting the maximum SPT number
  DWORD* attempts[5] = {NULL};
  BYTE idx;
  BYTE idxToUse = (BYTE)-1; // unsure, yet
  
  for (idx = 0; idx < 5; idx++)
  {
    // distinguish if there are missing sector IDs in the table
    // no gaps: observedSPT == maximumSPT
    BYTE observedSPT = 0;
    BYTE maximumSPT = 0;
    
    BYTE computeSPT = 0;
    WORD idxSPTComputer = 0;
    
    attempts[idx] = wdc->fillSectorsTable(tableCount);    
    const DWORD* sectorsTable = attempts[idx];
    if (!sectorsTable)
    {
      break; // memory error
    }
    
    for (WORD index = 0; index < tableCount; index++)
    {
      const DWORD& data = sectorsTable[index];
      if (data == 0xFFFFFFFFUL) // all four bytes undefined: cylinder number lo-hi, sector number, SDH
      {
        continue;
      }
      
      const BYTE sectorScanned = (BYTE)(data >> 16);
      const BYTE sdhScanned = (BYTE)(data >> 24);
      
      if (!cylinderMismatch)      
      {
        cylinderMismatch = (WORD)data != wdc->getPhysicalCylinder(); // loword: cylinder number 
      }      
      if (!headMismatch)
      {
        headMismatch = wdc->getPhysicalHead() != (sdhScanned & 0xF); // head number in SDH
      }
      if (!variableSectorSize)
      {
        variableSectorSize = (sdh & 0x9F) != (sdhScanned & 0x9F); // sector size code
      }
         
      // determine sectors per track
      if (idxSPTComputer == 0)
      {
        // count number of times until we see the same sector again: that's the observed SPT
        computeSPT = sectorScanned;
      }
      else if (computeSPT == sectorScanned)
      {
        computeSPT = 0; // stop counting
      }
      if (computeSPT)
      {
        observedSPT++;
      }
      
      // the maximum logical sector value seen
      if (sectorScanned > maximumSPT)
      {
        maximumSPT = sectorScanned;
      }
      idxSPTComputer++;
    }
    
    // failsafe    
    if (observedSPT > maximumSPT)
    {
      observedSPT = maximumSPT;
    }
    
    // no gaps in the sectors being scanned, done
    if ((observedSPT == maximumSPT) && (observedSPT > 0))
    {
      sectorsPerTrack = observedSPT;
      idxToUse = idx;
      break;
    }
    
    // gaps present, retry
    if (observedSPT > sectorsPerTrack)
    {
      sectorsPerTrack = observedSPT;
      idxToUse = idx;
    }      
  }
  
  // now determine which index to use, if ever
  DWORD* result = NULL;
  for (idx = 0; idx < 5; idx++)
  {
    if (attempts[idx])
    {
      if (idx == idxToUse)
      {
        result = attempts[idx];
        continue;
      }
      
      delete[] attempts[idx]; // and deallocate the rest
    }
  }
  
  return result;  
}

BYTE CalculateInterleave(const DWORD* sectorsTable, WORD tableCount, BYTE sectorsPerTrack)
{
  // sequential
  if (sectorsPerTrack < 3)
  {
    return 1;
  }
  
  BYTE thisSector = 0;
  BYTE nextSector = 0;
  WORD idx = 0;
  while (idx < tableCount-1)
  {
    thisSector = (BYTE)(sectorsTable[idx] >> 16);
    nextSector = (BYTE)(sectorsTable[idx+1] >> 16);
    if (sectorsTable[idx] == 0xFFFFFFFFUL) // undefined
    {
      idx += 2;
      continue;
    }

    // neither of these is the last sector
    if ((thisSector != sectorsPerTrack) && (nextSector != sectorsPerTrack))
    {
      break;
    }
    idx++;
  }
  
  if (idx < tableCount-1)
  {
    if ((nextSector - thisSector) == 1)
    {
      // confirmed sequential
      return 1;
    }
    
    // nope, try to compute from thisSector
    WORD idx2 = idx;
    BYTE interleave = 0;
    while (idx2 < tableCount)
    {
      if ((BYTE)(sectorsTable[idx2++] >> 16) != thisSector+1)
      {
        interleave++;
      }
      else
      {
        break;
      }
    }
    // double-check if nextSector is advanced by the same factor
    if ((BYTE)(sectorsTable[idx+1+interleave] >> 16) == nextSector+1)
    {
      return interleave;
    }
  }
  
  // could not determine interleave, fall back to sequential
  return 1;
}

