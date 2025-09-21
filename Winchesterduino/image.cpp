// Winchesterduino (c) 2025 J. Bogin, http://boginjr.com
// WDI disk imaging

#include "config.h"

// forward decl's
bool DoReadDisk();
bool DoWriteDisk();
bool DoVerifyParamsFromImage();
bool DoDetectMemoryCard();
bool DoPickFile(bool writingImage);

File doImageFile;
BYTE doProgmemResponseStr      = Progmem::uiEmpty;
BYTE doParams[32]              = {0};
BYTE doBuffer[1024]            = {0}; // one disk sector buffer (max. 1K supported)
DWORD* doSectorsTable          = NULL;
DWORD doTotalDataErrors        = 0;
DWORD doTotalCorrectedErrors   = 0;
DWORD doTotalBadBlocks         = 0;
DWORD doUnreadableTracks       = 0;
// write image to disk options:
bool doWriteImgOverrideParams  = false;
BYTE doWriteImgBadSectorMode   = 0; // 0: bad sectors formatted empty, 1: bad sectors formatted as bad
BYTE doWriteImgDataErrorsMode  = 0; // 0: CRC/ECC data errors formatted empty, 1: formatted as bad, 2: written (as good data)

// reuse the DOS inspect command path buffers to address filenames on the microSD card
extern BYTE path[MAX_PATH+1];
extern BYTE addPath[MAX_PATH+1];
int res; // FatFile.read() result

void CommandReadImage()
{ 
  if (!DoDetectMemoryCard())
  {
    return;
  }
  
  ui->print(Progmem::getString(Progmem::uiEscGoBack));
  
  // ask to image part of the disk
  BYTE key = 0;
  wdc->getParams()->PartialImage = false;
  wdc->getParams()->PartialImageStartCyl = 0;
  wdc->getParams()->PartialImageEndCyl = 0;
  
  if (wdc->getParams()->Cylinders > 1)
  {
    ui->print(Progmem::getString(Progmem::imgReadWholeDisk));
    key = toupper(ui->readKey("YN\e"));
    if (key == '\e')
    {
      ui->print(Progmem::getString(Progmem::uiNewLine));
      return;
    }
    
    const bool partialImage = (key == 'N');
    ui->print(Progmem::getString(Progmem::uiEchoKey), key);
    
    if (partialImage)
    {
      // start and end cylinder
      WORD startCylinder = 0;
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
      
      if ((startCylinder > 0) || (endCylinder != wdc->getParams()->Cylinders-1))
      {
        wdc->getParams()->PartialImage = true;
        wdc->getParams()->PartialImageStartCyl = startCylinder;
        wdc->getParams()->PartialImageEndCyl = endCylinder;
      }
    }
  }
    
  // choose file
  if (!DoPickFile(true))
  {
    return;
  }  
  
  // use the WDC SRAM buffer to write file description and comment
  wdc->sramBeginBufferAccess(true, 0);
  const BYTE* header = Progmem::getString(Progmem::imgWriteHeader);
  WORD len = strlen(header);
  for (BYTE index = 0; index < len; index++)
  {
    wdc->sramWriteByteSequential(header[index]);
  }
    
  ui->print(Progmem::getString(Progmem::imgWriteComment), MAX_PROMPT_LEN);
  ui->print(Progmem::getString(Progmem::imgWriteDone));
  ui->print(Progmem::getString(Progmem::imgWriteEnterEsc));
  key = ui->readKey("\r\e");
  ui->print(Progmem::getString(Progmem::uiDeleteLine));
  if (key == '\r')
  {
    bool emptyLine = false;
    
    // must incl. EOF and NUL
    while ((len + MAX_PROMPT_LEN + 2) < 32768)
    {
      const BYTE* promptBuffer = ui->prompt();
      WORD promptLen = strlen(promptBuffer);
      
      // done?
      if (!promptLen && emptyLine)
      {
        break;
      }    
      emptyLine = promptLen == 0;
      
      for (WORD index = 0; index < promptLen; index++)
      {
        wdc->sramWriteByteSequential(promptBuffer[index]);
      }
      wdc->sramWriteByteSequential(0x0D); // CR
      wdc->sramWriteByteSequential(0x0A); // LF
      len += promptLen + 2;

      ui->print(Progmem::getString(Progmem::uiNewLine)); 
    }
  }
  
  // EOF marks the end of header
  wdc->sramWriteByteSequential(0x1A);
  wdc->sramFinishBufferAccess();
  
  // copy current disk drive parameters
  memcpy(&doParams[0], wdc->getParams(), sizeof(WD42C22::DiskDriveParams));
  
  // seek to the beginning
  if (!wdc->getParams()->PartialImage)
  {
    wdc->seekDrive(0, 0);
  }
  else
  {
    wdc->seekDrive(wdc->getParams()->PartialImageStartCyl, 0);
  }  
  
  doTotalDataErrors = 0;
  doTotalCorrectedErrors = 0;
  doTotalBadBlocks = 0;
  doUnreadableTracks = 0;
  doProgmemResponseStr = Progmem::uiEmpty;
  ui->print(Progmem::getString(Progmem::uiNewLine));
  
  // read into file
  const bool success = DoReadDisk();
  doImageFile.close();
  sd.end();
  wdc->sramFinishBufferAccess();
  if (doSectorsTable)
  {
    delete[] doSectorsTable;
    doSectorsTable = NULL;
  }
  wdc->selectDrive(false);
   
  ui->print(Progmem::getString(Progmem::uiDeleteLine));
  ui->print(Progmem::getString(success ? Progmem::imgXferEnd : Progmem::imgXferFail));
  ui->print(Progmem::getString(Progmem::uiNewLine));
  if (doProgmemResponseStr)
  {
    ui->print(Progmem::getString(doProgmemResponseStr));
    ui->print(Progmem::getString(Progmem::uiNewLine));
  }
   
  // show disk stats
  if (success)
  {
    ui->print(Progmem::getString(Progmem::uiNewLine));
    ui->print(Progmem::getString(Progmem::imgDiskStats));
    ui->print(Progmem::getString(Progmem::imgBadBlocks), doTotalBadBlocks);
    ui->print(Progmem::getString(Progmem::imgBadTracks), doUnreadableTracks);
    if (wdc->getParams()->DataVerifyMode != MODE_CRC_16BIT)
    {
      ui->print(Progmem::getString(Progmem::imgDataCorrected), doTotalCorrectedErrors);  
    }    
    ui->print(Progmem::getString(Progmem::imgDataErrors), doTotalDataErrors);
  }
  
  ui->print(Progmem::getString(Progmem::uiNewLine));
  ui->print(Progmem::getString(Progmem::uiContinue));
  ui->readKey("\r");  
  ui->print(Progmem::getString(Progmem::uiNewLine));
}

void CommandWriteImage()
{
  if (!DoDetectMemoryCard())
  {
    return;
  }
  
  // override current disk parameters with those from the image?
  ui->print(Progmem::getString(Progmem::imgOverrideWrite1));
  ui->print(Progmem::getString(Progmem::imgOverrideWrite2));
  ui->print(Progmem::getString(Progmem::uiEscGoBack));
  ui->print(Progmem::getString(Progmem::imgOverrideWrite3));
  BYTE key = toupper(ui->readKey("YN\e"));
  if (key == '\e')
  {
    ui->print(Progmem::getString(Progmem::uiNewLine));
    return;
  }
  ui->print(Progmem::getString(Progmem::uiEchoKey), key);
  doWriteImgOverrideParams = (key == 'Y');
  
  // if not, ask whether to work on the whole disk or partial image
  wdc->getParams()->PartialImage = false;
  wdc->getParams()->PartialImageStartCyl = 0;
  wdc->getParams()->PartialImageEndCyl = 0;
  
  if (!doWriteImgOverrideParams && (wdc->getParams()->Cylinders > 1))
  {
    ui->print(Progmem::getString(Progmem::imgWriteWholeDisk));
    key = toupper(ui->readKey("YN\e"));
    if (key == '\e')
    {
      ui->print(Progmem::getString(Progmem::uiNewLine));
      return;
    }
    
    const bool partialImage = (key == 'N');
    ui->print(Progmem::getString(Progmem::uiEchoKey), key);
    
    if (partialImage)
    {
      // start and end cylinder
      WORD startCylinder = 0;
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
      
      if ((startCylinder > 0) || (endCylinder != wdc->getParams()->Cylinders-1))
      {
        wdc->getParams()->PartialImage = true;
        wdc->getParams()->PartialImageStartCyl = startCylinder;
        wdc->getParams()->PartialImageEndCyl = endCylinder;
      }
    }
  }
  
  // what to do with bad blocks in image
  ui->print(Progmem::getString(Progmem::imgBadBloxOption1));
  ui->print(Progmem::getString(Progmem::imgBadBloxOption2));
  key = toupper(ui->readKey("EB\e"));
  if (key == '\e')
  {
    ui->print(Progmem::getString(Progmem::uiNewLine));
    return;
  }
  ui->print(Progmem::getString(Progmem::uiEchoKey), key);
  doWriteImgBadSectorMode = (key == 'B') ? 1 : 0;
  
  // what to do with data errors in image
  ui->print(Progmem::getString(Progmem::imgDataErrorsOpt1));
  ui->print(Progmem::getString(Progmem::imgDataErrorsOpt2));
  key = toupper(ui->readKey("EBG\e"));
  if (key == '\e')
  {
    ui->print(Progmem::getString(Progmem::uiNewLine));
    return;
  }
  ui->print(Progmem::getString(Progmem::uiEchoKey), key);
  switch(key)
  {
  case 'E':
    doWriteImgDataErrorsMode = 0;
    break;
  case 'B':
    doWriteImgDataErrorsMode = 1;
    break;
  case 'G':
    doWriteImgDataErrorsMode = 2;
    break;
  }
  
  // choose file
  if (!DoPickFile(false))
  {
    return;
  }     
  
  // make a backup of the current disk parameters struct
  WD42C22::DiskDriveParams backup;
  memcpy(&backup, wdc->getParams(), sizeof(WD42C22::DiskDriveParams));
  
  // seek to the beginning
  wdc->seekDrive(0, 0);
  
  doTotalDataErrors = 0;
  doTotalBadBlocks = 0;
  doUnreadableTracks = 0;
  doProgmemResponseStr = Progmem::uiEmpty;
  ui->print(Progmem::getString(Progmem::uiNewLine));
  
  // write disk from file
  const bool success = DoWriteDisk();
  doImageFile.close();
  sd.end();
  wdc->sramFinishBufferAccess();
  if (doSectorsTable)
  {
    delete[] doSectorsTable;
    doSectorsTable = NULL;
  }
  wdc->selectDrive(false);  
     
  ui->print(Progmem::getString(Progmem::uiDeleteLine));
  ui->print(Progmem::getString(success ? Progmem::imgXferEnd : Progmem::imgXferFail));
  ui->print(Progmem::getString(Progmem::uiNewLine));
  if (doProgmemResponseStr)
  {
    ui->print(Progmem::getString(doProgmemResponseStr));
    ui->print(Progmem::getString(Progmem::uiNewLine));
  }
   
  // show image and disk stats
  if (success)
  {
    ui->print(Progmem::getString(Progmem::uiNewLine));
    ui->print(Progmem::getString(Progmem::imgImageStats));
    ui->print(Progmem::getString(Progmem::imgBadBlocks), doTotalBadBlocks);
    ui->print(Progmem::getString(Progmem::imgBadTracks), doUnreadableTracks); 
    ui->print(Progmem::getString(Progmem::imgDataErrors), doTotalDataErrors);
    ui->print(Progmem::getString(Progmem::imgRunScan));    
  }
  
  ui->print(Progmem::getString(Progmem::uiNewLine));
  ui->print(Progmem::getString(Progmem::uiContinue));
  ui->readKey("\r");  
  ui->print(Progmem::getString(Progmem::uiNewLine));
  
  // ask to restore back original disk parameters, if changed
  if (doWriteImgOverrideParams && (memcmp(wdc->getParams(), &backup, sizeof(WD42C22::DiskDriveParams)) != 0))
  {
    ui->print(Progmem::getString(Progmem::imgRestoreParams));
    key = toupper(ui->readKey("RK"));
    ui->print(Progmem::getString(Progmem::uiEchoKey), key);
    
    // restore from backup
    if (key == 'R')
    {
      memcpy(wdc->getParams(), &backup, sizeof(WD42C22::DiskDriveParams));
      wdc->applyParams();
    }    
  }
}

// read disk into file
bool DoReadDisk()
{
  
  // write WDI header and comment from SRAM to output file        
  wdc->sramBeginBufferAccess(false, 0);
  for (;;)
  {
    doBuffer[0] = wdc->sramReadByteSequential();
    SD_WRITE(1);
    
    if (doBuffer[0] == 0x1A)
    {          
      wdc->sramFinishBufferAccess();          
      break;
    }
  }
    
  // write DiskDriveParams to a total of 32 bytes; unused values pre-set to 0
  memcpy(&doBuffer[0], &doParams[0], sizeof(doParams));
  SD_WRITE(sizeof(doParams)); 
  
  ui->print(Progmem::getString(Progmem::scanProgress), wdc->getPhysicalCylinder());
  
  // process specified tracks
  for (;;)
  {
    // current physical cylinder and head
    WORD currentCylinder = wdc->getPhysicalCylinder();
    memcpy(&doBuffer[0], &currentCylinder, sizeof(WORD));
    SD_WRITE(sizeof(WORD)); 
    
    BYTE currentHead = wdc->getPhysicalHead();
    doBuffer[0] = currentHead;
    SD_WRITE(1);    
           
    // sectors per track: get SDH byte, 5 attempts
    BYTE spt = 0;
    BYTE sdh = 0;
    BYTE attempts = 5;
    while (attempts)
    {
      WORD dummy;
      BYTE dummy2;        
      wdc->scanID(dummy, dummy2, sdh);
      
      // WDC timeout, drive not ready, writefault
      if (wdc->getLastError())
      {
        if (wdc->getLastError() < 4)
        {
          doProgmemResponseStr = wdc->getLastErrorMessage();
          return false;
        }
        
        attempts--;
      }
      else
      {
        break;
      }
    }
    
    if (doSectorsTable)
    {
      delete[] doSectorsTable;
      doSectorsTable = NULL;
    }
    
    bool cylindersMismatch = false;
    bool headsMismatch = false;
    bool variableSectorSize = false;
    
    WORD sectorsTableCount = 0;
    WORD sectorIdx = 0;
    WORD startingSectorIdx = (WORD)-1;
    WORD startingSector = 0;
    BYTE lastPos = 0;
    BYTE currentSector = 0;    
    
    // no error - now calculate SPT
    if (attempts)
    {
      doSectorsTable = CalculateSectorsPerTrack(sdh, spt, sectorsTableCount, headsMismatch, cylindersMismatch, variableSectorSize);
      if (!doSectorsTable && !sectorsTableCount)
      {
        ui->fatalError(Progmem::uiFeMemory);
        return false;
      }
      
      // SPT valid
      if (spt)
      {
        // as the interleave table almost always never starts from the beginning, find the starting sector
        // but even the starting sector number might not start from 0        
        while (startingSectorIdx == (WORD)-1) // undefined
        {
          bool found = false;
          
          for (WORD idx = 0; idx < sectorsTableCount; idx++)
          {                   
            if (doSectorsTable[idx] == 0xFFFFFFFFUL) // undefined?
            {
              continue;
            }
            
            // logical sector number matching?
            if ((BYTE)(doSectorsTable[idx] >> 16) == startingSector)
            {
              startingSectorIdx = idx;
              sectorIdx = startingSectorIdx;
              lastPos = 0; // use this as count how many were written in the map
              found = true;
              break;
            }
          }
          
          if (found)
          {
            break;
          }
          
          startingSector++;  // starts from 1, 2 or whatever
          if (startingSector > 255) // cannot sync
          {
            spt = 0; // mark track as unreadable
            break;
          }
        }
      }
    }      
    
    // store SPT
    doBuffer[0] = spt;
    SD_WRITE(1);
    
    // track contains no sectors?
    if (!spt)
    {
      doUnreadableTracks++;
            
      // seek to the next
      currentHead++;
      if (currentHead == wdc->getParams()->Heads)
      {
        currentHead = 0;
        currentCylinder++;
        
        ui->print(Progmem::getString(Progmem::scanProgress), currentCylinder);
      }
      if ((currentCylinder == wdc->getParams()->Cylinders) ||
          (wdc->getParams()->PartialImage && (currentCylinder-1 == wdc->getParams()->PartialImageEndCyl)))
      {
        doProgmemResponseStr = Progmem::uiEmpty;
        return true; // done
      }
      
      wdc->seekDrive(currentCylinder, currentHead);      
      continue; // transfer continues
    }
    
    // prepare sector numbering map
_j1: 
    while ((lastPos < spt) && (sectorIdx < sectorsTableCount))
    {
      if (doSectorsTable[sectorIdx] == 0xFFFFFFFFUL) // undefined?
      {
        sectorIdx++;
        continue;
      }
      
      currentSector = (BYTE)(doSectorsTable[sectorIdx] >> 16);
      memcpy(&doBuffer[0], &doSectorsTable[sectorIdx], sizeof(DWORD));
      SD_WRITE(sizeof(DWORD));
      
      lastPos++;
      sectorIdx++;
    }
    
    // sectors per track count not reached: do we still need to go from the beginning of the table?
    if ((sectorIdx == sectorsTableCount) && (lastPos < spt))
    {
      bool found = false;
          
      while (currentSector && !found)
      {
        for (sectorIdx = 0; sectorIdx < sectorsTableCount; sectorIdx++)
        {
          if ((((BYTE)(doSectorsTable[sectorIdx] >> 16)) == currentSector) &&
               (doSectorsTable[sectorIdx] != 0xFFFFFFFFUL))
          {
            found = true;
            break;
          }
        }
        
        if (!found)
        {
          currentSector++; // possible gap?
        }
      }            
      
      // found from the beginning, get the succeeding sector index
      if (found)
      {
        sectorIdx += 1;
        if (sectorIdx < sectorsTableCount)
        {
          goto _j1; // valid
        }
      }

      // not found or out-of-bounds
      sectorIdx = 0;
      goto _j1;
    }
    
    sectorIdx = startingSectorIdx;
    lastPos = 0;
    currentSector = 0;
    WORD secSizeBytes = 0;    
    
    // now try to read
    bool readSectorBySector = false; // true: read whole track
    if (headsMismatch || cylindersMismatch || variableSectorSize)
    {
      readSectorBySector = true;
    }
    else // try to read whole track in one go
    {
      secSizeBytes = wdc->getSectorSizeFromSDH((BYTE)(doSectorsTable[startingSectorIdx] >> 24));
      
      if ((DWORD)secSizeBytes*spt > 32768) // the WD42C22 can address a maximum of 32K
      {
        readSectorBySector = true;
      }
      else
      {
        wdc->readTrack(spt, secSizeBytes, (BYTE)(doSectorsTable[startingSectorIdx] >> 16));
        
        if (wdc->getLastError())
        {
          if (wdc->getLastError() < 4) // WDC timeout, drive not ready, writefault
          {
            doProgmemResponseStr = wdc->getLastErrorMessage();
            return false;
          }
          
          readSectorBySector = true; // fallback due to bad sectors, gaps, etc.
        }
      }      
    }

_j2:
    while ((lastPos < spt) && (sectorIdx < sectorsTableCount))
    {     
      if (doSectorsTable[sectorIdx] == 0xFFFFFFFFUL) // skip undefined
      {
        sectorIdx++;
        continue;
      }
      
      const BYTE logicalSector = (BYTE)(doSectorsTable[sectorIdx] >> 16);
      currentSector = logicalSector;
      BYTE sectorDataType = 0;
      
      if (!readSectorBySector) // whole track
      {
        sectorDataType = 1; // read call went without errors; set as valid
      }
      else // single sectors
      {
        const BYTE sdh = (BYTE)(doSectorsTable[sectorIdx] >> 24);        
        secSizeBytes = wdc->getSectorSizeFromSDH(sdh);              
        const BYTE logicalHead = sdh & 0xF;        
        const WORD logicalCylinder = (WORD)doSectorsTable[sectorIdx];      
        
        wdc->readSector(logicalSector, secSizeBytes, false, &logicalCylinder, &logicalHead);     
        if (wdc->getLastError())
        {
          if (wdc->getLastError() < 4) // WDC timeout, drive not ready, writefault
          {
            doProgmemResponseStr = wdc->getLastErrorMessage();
            return false;
          }
          
          else if (wdc->getLastError() == WDC_CORRECTED) // treat successful ECC correction as OK
          {
            sectorDataType = 1;
            doTotalCorrectedErrors++;
          }
        
          else if (wdc->getLastError() == WDC_DATAERROR) // we have data, but likely faulty
          {
            sectorDataType = 2;
            doTotalDataErrors++;
          }
          
          else // no data in buffer
          {
            sectorDataType = 0;
            doTotalBadBlocks++;
          }
        }
        else
        {
          sectorDataType = 1; // valid data
        }
      }
      
      // determine whether to compress the data
      if (sectorDataType)
      {
        WORD offset = 0;
        if (!readSectorBySector) // whole track already read in SRAM, setup proper offset
        {
          offset = (logicalSector-startingSector)*secSizeBytes;
        }
        
        wdc->sramBeginBufferAccess(false, offset);
        bool compressedData = true;
        BYTE lastData = wdc->sramReadByteSequential();
        
        for (WORD idx = 1; idx < secSizeBytes; idx++)
        {
          const BYTE currData = wdc->sramReadByteSequential();
          if (currData != lastData)
          {
            compressedData = false;
            break;
          }
          lastData = currData;
        }
               
        if (compressedData)
        {
          sectorDataType |= 0x80; //set bit 7
        }
        
        wdc->sramBeginBufferAccess(false, offset); // rewind SRAM buffer          
      }      
      
      // write sector data type byte
      doBuffer[0] = sectorDataType; 
      SD_WRITE(1);      
                
      // data is ready
      switch(sectorDataType)
      {
      case 1:
      case 2:      
      {
        WORD count = 0;
        while (count != secSizeBytes)
        {
          doBuffer[count++] = wdc->sramReadByteSequential();
        }
        SD_WRITE(count);
        wdc->sramFinishBufferAccess();
      }
      break;
      case 0x81:
      case 0x82:      
      {
        // compressed data (same byte repeated secSizeBytes)
        doBuffer[0] = wdc->sramReadByteSequential();
        SD_WRITE(1);
        wdc->sramFinishBufferAccess();
      }
      break;
      }
      
      // next sector
      sectorIdx++;
      lastPos++;
    }    
    if ((sectorIdx == sectorsTableCount) && (lastPos < spt))
    {
      bool found = false;
            
      while (currentSector && !found)
      {
        for (sectorIdx = 0; sectorIdx < sectorsTableCount; sectorIdx++)
        {
          if ((((BYTE)(doSectorsTable[sectorIdx] >> 16)) == currentSector) &&
               (doSectorsTable[sectorIdx] != 0xFFFFFFFFUL))
          {
            found = true;
            break;
          }
        }
        
        if (!found)
        {
          currentSector++;
        }
      }            
      
      if (found)
      {
        sectorIdx += 1;
        if (sectorIdx < sectorsTableCount)
        {
          goto _j2;
        }
      }
      
      sectorIdx = 0;
      goto _j2;
    }
       
    // end of track?
    doProgmemResponseStr = Progmem::uiEmpty;
    
    // and seek to next
    currentHead++;
    if (currentHead == wdc->getParams()->Heads)
    {
      currentHead = 0;
      currentCylinder++;
      
      ui->print(Progmem::getString(Progmem::scanProgress), currentCylinder);
    }
    if ((currentCylinder == wdc->getParams()->Cylinders) ||    
        (wdc->getParams()->PartialImage && (currentCylinder-1 == wdc->getParams()->PartialImageEndCyl)))
    {
      doProgmemResponseStr = Progmem::uiEmpty;
      return true; // done
    }

    wdc->seekDrive(currentCylinder, currentHead);
  }
  
  // infinite loop
}

// write disk from file
bool DoWriteDisk()
{ 
  
  // first step: check the header of variable length; skip its contents (needs to end with EOF)
  // verify it begins with "WDI " otherwise abort
  SD_READ(4);
  if (memcmp(&doBuffer[0], "WDI ", 4) != 0)
  {
    doProgmemResponseStr = Progmem::imgXferErrHeader;
    return false;
  }
  
  // keep looking for ASCII EOF marking the end of header
  for (;;)
  {
    int eof = doImageFile.read();
    if (eof == -1)
    {
      doProgmemResponseStr = Progmem::imgXferErrHeader;
      return false;
    }
    else if (eof == 0x1A) // EOF
    {
      break;
    }
  }
 
  // now copy 32 bytes of drive table into our array and then check it for validity
  SD_READ(sizeof(doParams));
  memcpy(&doParams[0], &doBuffer[0], sizeof(doParams));
  if (!DoVerifyParamsFromImage())
  {
    return false; // response text prepared
  }
      
  // apply new parameters?
  if (doWriteImgOverrideParams)
  {
    memcpy(wdc->getParams(), &doParams[0], sizeof(WD42C22::DiskDriveParams));
    wdc->applyParams();
    
    if (wdc->getLastError())
    {
      doProgmemResponseStr = wdc->getLastErrorMessage();
      return false;
    }
  }
  
  // process tracks in image
  bool firstRun = true;
  ui->print(Progmem::getString(Progmem::uiOperationPending));
  
  for (;;)
  {
    if (doSectorsTable) // next track?
    {
      delete[] doSectorsTable;
      doSectorsTable = NULL;
    }
    
    if (doImageFile.curPosition() == doImageFile.fileSize()) // end-of-file
    {
      doProgmemResponseStr = Progmem::uiEmpty;
      return true;
    }
    
    // current physical cylinder
    SD_READ(sizeof(WORD));
    if (doBuffer[1] == 0x1A) // XMODEM end-of-file marker, transfer over
    {
      doProgmemResponseStr = Progmem::uiEmpty;
      return true;
    }
    
    WORD currentCylinder = 0;
    memcpy(&currentCylinder, &doBuffer[0], sizeof(WORD));
    if (currentCylinder >= wdc->getParams()->Cylinders) // check if within bounds
    {
      doProgmemResponseStr = Progmem::imgXferErrCyls;
      return false;
    }
    
    // head
    SD_READ(1);
    BYTE currentHead = doBuffer[0];
    if (currentHead >= wdc->getParams()->Heads)
    {
      doProgmemResponseStr = Progmem::imgXferErrHeads;
      return false;
    }
    
    // sectors per track
    SD_READ(1);
    BYTE spt = doBuffer[0];
    if (!spt)
    {
      doUnreadableTracks++;
      continue;
    }
    
    // write partial image: skip over the sectors table and data?
    static bool partialImageSkipData = false;
    if (wdc->getParams()->PartialImage && 
       ((currentCylinder < wdc->getParams()->PartialImageStartCyl) || (currentCylinder > wdc->getParams()->PartialImageEndCyl)))
    {
      // change "Processing cylinder X..." to "Busy..."
      if (!partialImageSkipData)
      {
        ui->print(Progmem::getString(Progmem::uiDeleteLine));
        ui->print(Progmem::getString(Progmem::uiOperationPending));        
        partialImageSkipData = true;
      }
    }
    else
    {
      partialImageSkipData = false;
    }
    
    // read and interpret sectors table
    SD_READ(spt*sizeof(DWORD));
    doSectorsTable = new DWORD[spt];
    if (!doSectorsTable)
    {
      ui->fatalError(Progmem::uiFeMemory);
      return false;
    }
    memcpy(doSectorsTable, &doBuffer[0], spt*sizeof(DWORD));
    
    // skip over all data records?
    if (partialImageSkipData)
    {
      for (BYTE sector = 0; sector < spt; sector++)
      {
        SD_READ(1);
        const BYTE sectorDataType = doBuffer[0];
        
        if (!sectorDataType)
        {
          continue; // no data record follows
        }
        else if (sectorDataType & 0x80)
        {
          SD_READ(1); // 1 byte of compressed data
        }
        else
        {
          const BYTE sdh = (BYTE)(doSectorsTable[sector] >> 24);
          SD_READ(wdc->getSectorSizeFromSDH(sdh)); // X bytes of raw data
        }
      }
      
      // go to next track
      continue;
    }

    // progress indicator
    if (firstRun || (currentCylinder != wdc->getPhysicalCylinder()))
    {
      firstRun = false;
      ui->print(Progmem::getString(Progmem::scanProgress), currentCylinder);
    }
    
    // since we need to format, and set gaps, make sure there are no variable size sectors,
    // and that the logical cylinder and head numbers do not differ between each other.
    // -> the Format Track command of the WD42C22 has no provision of customizing these between each,
    // as the value is taken from a task register, for the whole track.
    // ...otherwise we would have to call writeID to overwrite each sector ID and risk losing data,
    // as this command requires a precise byte offset where to write the changes...   
    const BYTE sdh = (BYTE)(doSectorsTable[0] >> 24); // data of the first sector in the table
    const WORD logicalCylinder = (WORD)doSectorsTable[0];
    const BYTE logicalHead = sdh & 0xF;        
    const WORD secSizeBytes = wdc->getSectorSizeFromSDH(sdh);    
    
    // inspect the first logical sector and verify the rest
    wdc->sramBeginBufferAccess(true, 0);
    wdc->sramWriteByteSequential(0);
    wdc->sramWriteByteSequential((BYTE)(doSectorsTable[0] >> 16));
    
    for (WORD idx = 1; idx < spt; idx++)
    {
      const BYTE thisSdh = (BYTE)(doSectorsTable[idx] >> 24);        
      if (wdc->getSectorSizeFromSDH(thisSdh) != secSizeBytes)
      {
        doProgmemResponseStr = Progmem::imgXferErrVar1;
        return false;
      }
      
      const WORD thisCylinder = (WORD)doSectorsTable[idx];
      const BYTE thisHead = thisSdh & 0xF;
      if ((thisHead != logicalHead) || (thisCylinder != logicalCylinder))
      {
        doProgmemResponseStr = Progmem::imgXferErrVar2;
        return false;
      }
      
      // create format interleave table, set good sectors and later in the datastream, find out which ones are bad
      wdc->sramWriteByteSequential(0);
      wdc->sramWriteByteSequential((BYTE)(doSectorsTable[idx] >> 16));
    }
    wdc->sramFinishBufferAccess();
    
    // prepare for writing, seek the drive and format
    wdc->seekDrive(currentCylinder, currentHead);
    wdc->formatTrack(spt, secSizeBytes, &logicalCylinder, &logicalHead);
    
    // formatTrack can only fail with WDC timeout, drive not ready or write fault
    if (wdc->getLastError())
    {
      doProgmemResponseStr = wdc->getLastErrorMessage();
      return false;
    }      
    
    // determine whether to write the whole track at once, or go sector by sector:
    // write whole track if there were no bad or unreadable sectors, based on sectorDataType
    bool writeSectorBySector = false;
    const DWORD dataRecords = doImageFile.curPosition(); // at the start of data records

    if ((DWORD)secSizeBytes*spt > 32768) // I could only fit around 10K per track onto an MFM drive regardless
    {
      writeSectorBySector = true;
    }
    else
    {
      for (BYTE sector = 0; sector < spt; sector++)
      {
        SD_READ(1);
        BYTE sectorDataType = doBuffer[0];      
        if (!sectorDataType || ((sectorDataType & 0x7F) > 1))
        {
          writeSectorBySector = true;
          break;
        }
        SD_READ(sectorDataType & 0x80 ? 1 : secSizeBytes);
      }  
    }        
    
    if (!writeSectorBySector)
    {
      // prepare whole track buffer
      doProgmemResponseStr = Progmem::uiEmpty;
      doImageFile.seekSet(dataRecords);
      wdc->sramBeginBufferAccess(true, 0);
      
      for (BYTE sectorIdx = 0; sectorIdx < spt; sectorIdx++)
      {
        // logical sector number, then data
        wdc->sramWriteByteSequential((BYTE)(doSectorsTable[sectorIdx] >> 16));
        
        SD_READ(1);
        const BYTE sectorDataType = doBuffer[0];
        
        if (sectorDataType & 0x80)
        {
          SD_READ(1); // compressed data
          for (WORD count = 0; count < secSizeBytes; count++)
          {
            wdc->sramWriteByteSequential(doBuffer[0]);
          }
        }
        
        else // normal data
        {
          SD_READ(secSizeBytes);
          for (WORD idx = 0; idx < secSizeBytes; idx++)
          {
            wdc->sramWriteByteSequential(doBuffer[idx]);
          }
        }
      }
      
      wdc->sramFinishBufferAccess();
      wdc->writeTrack(spt, secSizeBytes, &logicalCylinder, &logicalHead);
      
      if (wdc->getLastError())
      {
        if (wdc->getLastError() < 4) // WDC timeout, drive not ready, writefault
        {
          doProgmemResponseStr = wdc->getLastErrorMessage();
          return false;
        }
      }
      else
      {
        // go to next track
        continue;
      }
    }

    // fall back to single sector write
    doProgmemResponseStr = Progmem::uiEmpty;
    doImageFile.seekSet(dataRecords);    
        
    BYTE sectorIdx = 0;
    while (sectorIdx < spt)
    {     
      // determine what to write - sector data type; check validity
      SD_READ(1);
      BYTE sectorDataType = doBuffer[0];
      if ((sectorDataType & 0x7F) > 2)
      {
        doProgmemResponseStr = Progmem::imgXferErrSecTyp;
        return false;
      }
      
      const BYTE sdh = (BYTE)(doSectorsTable[sectorIdx] >> 24);
      const WORD logicalCylinder = (WORD)doSectorsTable[sectorIdx];
      const BYTE logicalHead = sdh & 0xF;
      const BYTE logicalSector = (BYTE)(doSectorsTable[sectorIdx] >> 16);
      
      // unreadable sector
      if (sectorDataType == 0)
      {
        // already formatted empty...
        if (doWriteImgBadSectorMode == 1) // also flag as bad?
        {
          wdc->setBadSector(logicalSector, &logicalCylinder, &logicalHead);
        }
        
        // continue with the next
        doTotalBadBlocks++;
        sectorIdx++;
      }
      
      // data follows
      else
      {
        bool doNotWrite = false; 
        bool formatBad = false;  
        
        // contains CRC/ECC error?
        if ((sectorDataType & 0x7F) == 2)
        { 
          if (doWriteImgDataErrorsMode == 0)
          {
            doNotWrite = true; // just keep formatted empty
          }
          else if (doWriteImgDataErrorsMode == 1)
          {
            doNotWrite = true;
            formatBad = true; // do not write and set sector ID as bad
          }
        }
        
        // initialize SRAM buffer write
        if (!doNotWrite && !formatBad)
        {
          wdc->sramBeginBufferAccess(true, 0);
        }
        
        // all data are of the same byte - compressed
        if (sectorDataType & 0x80)
        {
          SD_READ(1)
          BYTE compressed = doBuffer[0];
          
          // since we format every track, before writing a sector,
          // and the WD42C22 initializes every sector to 0xFF during formatting,
          // (WD42C22A datasheet page 55, Format Track (Cont.) "Data bytes are FF."),
          // thus, set the "do not write" flag to save time, because this value is already written
          if (compressed == 0xFF)
          {
            doNotWrite = true;
          }
          
          if (!doNotWrite)
          {
            for (WORD index = 0; index < secSizeBytes; index++)
            {
              wdc->sramWriteByteSequential(compressed);
            }            
          }
          
          wdc->sramFinishBufferAccess();          
        }
        
        // normal data
        else
        {
          SD_READ(secSizeBytes);
          if (!doNotWrite)
          {
            for (WORD idx = 0; idx < secSizeBytes; idx++)
            {
              wdc->sramWriteByteSequential(doBuffer[idx]);
            }  
          }
          
          wdc->sramFinishBufferAccess();
        }
          
        if (!doNotWrite)  
        {
          // write
          wdc->writeSector(logicalSector, secSizeBytes, &logicalCylinder, &logicalHead);     
          if (wdc->getLastError())
          {
            if (wdc->getLastError() < 4) // WDC timeout, drive not ready, writefault
            {
              doProgmemResponseStr = wdc->getLastErrorMessage();
              return false;
            }
          }
        }
        
        // or format as bad
        if (formatBad)
        {
          wdc->setBadSector(logicalSector, &logicalCylinder, &logicalHead);
        }
        
        // count errors
        if ((sectorDataType & 0x7F) == 2)
        {
          doTotalDataErrors++;
        }
          
        // next sector      
        sectorIdx++;        
      }      
    }
  }
  
  // infinite loop
}

bool DoVerifyParamsFromImage()
{ 
  // assume error
  BYTE backup = doProgmemResponseStr;
  doProgmemResponseStr = Progmem::imgXferErrParams;
  
  // access loaded array as disk drive parameters POD
  WD42C22::DiskDriveParams* params = (WD42C22::DiskDriveParams*)(&doParams[0]);
  
  if (params->DataVerifyMode > MODE_ECC_56BIT)
  {
    return false;
  }
  if ((params->Cylinders == 0) || (params->Cylinders > 2048))
  {
    return false;
  }
  if ((params->Heads == 0) || (params->Heads > 16))
  {
    return false;
  }
  
  // "-1" or 65535 not considered as valid as these are turned on or off via a flag
  if ((params->WritePrecompStartCyl > 2048) || 
      (params->RWCStartCyl > 2048) ||
      (params->LandingZone > 2048) || 
      (params->PartialImageStartCyl >= params->Cylinders) ||
      (params->PartialImageEndCyl >= params->Cylinders))
  {
    return false;
  }
  
  // check if MFM-RLL mismatch (and we're not set to override parameters)
  if (!doWriteImgOverrideParams && (params->UseRLL != wdc->getParams()->UseRLL))
  {
    doProgmemResponseStr = Progmem::imgXferErrMFMRLL; // inform about mismatch
    return false;
  }
  
  // check partial image bounds
  if (wdc->getParams()->PartialImage)
  {
    doProgmemResponseStr = Progmem::imgXferErrPart;
    
    // sanity check
    if ((wdc->getParams()->PartialImageStartCyl > wdc->getParams()->PartialImageEndCyl) ||
        (params->PartialImageStartCyl > params->PartialImageEndCyl))
    {
      return false;
    }
    
    // nothing to do: the loaded image is already partial, and the supplied start/end bounds are out
    if ((params->PartialImageStartCyl > wdc->getParams()->PartialImageEndCyl) ||
        (params->PartialImage && (wdc->getParams()->PartialImageStartCyl > params->PartialImageEndCyl)))
    {
      return false;
    }
  }
  
  doProgmemResponseStr = backup; // alles in Ordnung
  return true;
}

bool DoDetectMemoryCard()
{ 
  sd.end();
  
  static SdSpiConfig cfg(53, USER_SPI_BEGIN); //53: SCS 
  if (!sd.cardBegin(cfg))
  {
    ui->print(Progmem::getString(Progmem::uiNewLine));
    ui->print(Progmem::getString(Progmem::imgXferCardMissing));
    ui->print(Progmem::getString(Progmem::uiNewLine));
    return false;
  }
  
  if (!sd.card()->sectorCount() || !sd.volumeBegin())
  {
    sd.end();
    
    ui->print(Progmem::getString(Progmem::uiNewLine));
    ui->print(Progmem::getString(Progmem::imgXferCardError));
    ui->print(Progmem::getString(Progmem::uiNewLine));
    return false;
  }
  
  return true;  
}

bool DoPickFile(bool writingImage)
{
  const char wdiExt[] = ".wdi";
  
  if (!DoDetectMemoryCard())
  {
    return false;
  }
  
  // try to mount root directory
  strcpy(path, "/");
  File rootdir = sd.open(path, O_RDONLY);
  if (!rootdir)
  {
    ui->print(Progmem::getString(Progmem::uiNewLine));
    ui->print(Progmem::getString(Progmem::imgXferCardError));
    ui->print(Progmem::getString(Progmem::uiNewLine));
    return false;
  }
  
  ui->print(Progmem::getString(Progmem::imgDirListing));
  
  bool noFiles = true;
  while(true)
  {
    File file = rootdir.openNextFile(O_RDONLY);
    if (!file)
    {
      break;
    }

    if (file.isHidden())
    {
      file.close();
      continue;
    }
    
    // filter directories and *.wdi
    addPath[0] = 0;
    file.getName(addPath, MAX_PATH);  
    const bool isDirectory = file.isDir() || file.isSubDir();
    file.close();
    if (!strlen(addPath))
    {
      continue;
    }    
    if (!isDirectory)
    {
      const BYTE* ext = strcasestr(addPath, wdiExt);
      if (!ext)
      {
        continue;
      }
      
      // also ending with it?
      const BYTE extPos = ext-&addPath[0];
      if (extPos != strlen(addPath)-4)
      {
        continue;
      }
    }
    
    // do directory listing
    noFiles = false;
    ui->print(isDirectory ? "<DIR> " : "      ");
    ui->print("%s", addPath);
    ui->print(Progmem::getString(Progmem::uiNewLine));
  }
  rootdir.close();
  
  if (noFiles)
  {
    ui->print(Progmem::getString(Progmem::dosDirectoryEmpty));
    ui->print(Progmem::getString(Progmem::uiNewLine));
    
    if (!writingImage)
    {
      ui->print(Progmem::getString(Progmem::uiContinue));
      ui->readKey("\r");  
      ui->print(Progmem::getString(Progmem::uiNewLine));
      return false;  
    }    
  }
  
  while(true)
  {
    strcpy(path, "/");
    ui->print(Progmem::getString(writingImage ? Progmem::imgDirPickWrite : Progmem::imgDirPickRead));
    const BYTE* promptBuffer = ui->prompt(MAX_PATH-1, NULL, true); // -1 to account for initial '/' in path buffer
    if (!promptBuffer) // cancelled
    {
      ui->print(Progmem::getString(Progmem::uiNewLine));
      return false;
    }
    else if (strlen(promptBuffer) == 0)
    {
      continue;
    }
    
    WORD idx = 0;
    bool emptyString = true;
    while (idx < strlen(promptBuffer))
    {
      if (!isspace(promptBuffer[idx++]))
      {
        emptyString = false;
        break;
      }
    }
    if (emptyString)
    {
      continue;
    }    
    ui->print(Progmem::getString(Progmem::uiNewLine));
    
    // append .wdi extension, if not specified
    bool appendExt = false;
    strcat(path, promptBuffer);  
    const BYTE* ext = strcasestr(path, wdiExt);
    if (!ext)
    {
      appendExt = true;
    }
    else
    {
      const BYTE extPos = ext-&path[0];
      if (extPos != strlen(path)-4)
      {
        appendExt = true;
      }
    }  
    if (appendExt)
    {
      if (strlen(path) > MAX_PATH-4)
      {
        path[MAX_PATH-4] = 0; // make space :)
      }
      strcat(path, wdiExt);
    }
    
    // check if file exists; if writing, ask to overwrite
    if (!DoDetectMemoryCard())
    {
      return false;
    }
    
    File testOpen = sd.open(path, O_RDONLY);
    bool fileExists = false;
    if (testOpen)
    {
      fileExists = true;
      testOpen.close();
    }
    if (!writingImage && !fileExists)
    {
      ui->print(Progmem::getString(Progmem::imgDirInvalid));
      continue;
    }
    else if (writingImage && fileExists)
    {
      ui->print(Progmem::getString(Progmem::imgDirOverwrite));
      BYTE key = toupper(ui->readKey("YN\e"));
      if (key == '\e')
      {
        ui->print(Progmem::getString(Progmem::uiNewLine));
        return false;
      }
      
      ui->print(Progmem::getString(Progmem::uiEchoKey), key);
      if (key == 'N')
      {
        continue;
      }
    }
    
    // open the file for read or write
    doImageFile = sd.open(path, writingImage ? O_WRITE | O_CREAT | O_TRUNC : O_RDONLY);
    if (!doImageFile)
    {
      ui->print(Progmem::getString(Progmem::uiNewLine));
      ui->print(Progmem::getString(Progmem::imgXferFileError));
      ui->print(Progmem::getString(Progmem::uiNewLine));
      ui->print(Progmem::getString(Progmem::uiContinue));
      ui->readKey("\r");  
      ui->print(Progmem::getString(Progmem::uiNewLine));
      return false;
    }

    return true;
  }
}