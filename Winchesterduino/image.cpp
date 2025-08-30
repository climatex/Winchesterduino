// Winchesterduino (c) 2025 J. Bogin, http://boginjr.com
// WDI disk imaging

#include "config.h"

// forward decl's
int  RX(int msDelay);
void TX(const char *data, int size);

// XMODEM callback related
void CbCleanup();
bool CbReadDisk(DWORD packetNo, BYTE* data, WORD size);
bool CbWriteDisk(DWORD packetNo, BYTE* data, WORD size);
bool CbVerifyParamsFromImage();

// values not modified by CbCleanup()
BYTE cbProgmemResponseStr      = 0;
bool cbSuccess                 = false;
DWORD cbTotalDataErrors        = 0;
DWORD cbTotalCorrectedErrors   = 0;
DWORD cbTotalBadBlocks         = 0;
DWORD cbUnreadableTracks       = 0;
// write image to disk options:
bool cbWriteImgOverrideParams  = false;
BYTE cbWriteImgBadSectorMode   = 0; // 0: bad sectors formatted empty, 1: bad sectors formatted as bad
BYTE cbWriteImgDataErrorsMode  = 0; // 0: CRC/ECC data errors formatted empty, 1: formatted as bad, 2: written (as good data)

// the rest, restored thru CbCleanup()
bool cbInProgress              = false;
bool cbProcessingHeader        = true;
bool cbProcessingDriveTable    = false;
bool cbCylinderSpecified       = false;
bool cbHeadSpecified           = false;
bool cbSptSpecified            = false;
bool cbSecMapSpecified         = false;
bool cbSecDataTypeSpecified    = false;
DWORD cbLastPos                = 0;
WORD cbCylinder                = 0;
BYTE cbHead                    = 0;
BYTE cbSpt                     = 0;
BYTE cbCurrentSector           = 0;
BYTE cbParams[32]              = {0};
DWORD* cbSectorsTable          = NULL;
WORD cbSectorsTableCount       = 0;
WORD cbSectorIdx               = 0;
WORD cbStartingSectorIdx       = (WORD)-1;
BYTE cbSectorDataType          = 0;
WORD cbSecSizeBytes            = 0;

void CommandReadImage()
{ 
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
  
  // ask to use 1K packets
  bool useXMODEM1K = false;
  BYTE* testAlloc = new BYTE[1030];
  if (testAlloc)
  {
    delete[] testAlloc;
    ui->print(Progmem::getString(Progmem::imgXmodem1k));
    key = toupper(ui->readKey("YN\e"));
    if (key == '\e')
    {
      ui->print(Progmem::getString(Progmem::uiNewLine));
      return;
    }
    ui->print(Progmem::getString(Progmem::uiEchoKey), key);
    useXMODEM1K = (key == 'Y');
  }
   
  ui->print(Progmem::getString(Progmem::uiNewLine));
  
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
    while ((len + MAX_PROMPT_LEN + 2) < 2048)
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
  wdc->sramBeginBufferAccess(false, 0); // prepare reading
  
  // copy current disk drive parameters
  memcpy(&cbParams, wdc->getParams(), sizeof(WD42C22::DiskDriveParams));
  
  // seek to the beginning
  if (!wdc->getParams()->PartialImage)
  {
    wdc->seekDrive(0, 0);
  }
  else
  {
    wdc->seekDrive(wdc->getParams()->PartialImageStartCyl, 0);
  }  
  
  ui->print("");
  ui->print(Progmem::getString(useXMODEM1K ? Progmem::imgXmodem1kPrefix : Progmem::imgXmodemPrefix));
  ui->print(Progmem::getString(Progmem::imgXmodemWaitRecv));
  ui->setPrintDisabled(true);
  
  // reset values that are not modified by CbCleanup()
  cbProgmemResponseStr = 0;
  cbSuccess = false;
  cbInProgress = true;
  cbTotalDataErrors = 0;
  cbTotalCorrectedErrors = 0;
  cbTotalBadBlocks = 0;
  cbUnreadableTracks = 0;
  
  // read and transmit
  XModem modem(RX, TX, &CbReadDisk, useXMODEM1K);
  modem.transmit();
  CbCleanup();
  DumpSerialTransfer();
  wdc->selectDrive(false);
   
  ui->setPrintDisabled(false); 
  ui->print("");   
  ui->print(Progmem::getString(Progmem::uiDeleteLine));
  ui->print(Progmem::getString(Progmem::uiVT100ClearScreen));  
    
  ui->print(Progmem::getString(cbSuccess ? Progmem::imgXmodemXferEnd : Progmem::imgXmodemXferFail));
  ui->print(Progmem::getString(Progmem::uiNewLine));
  if (cbProgmemResponseStr)
  {
    ui->print(Progmem::getString(cbProgmemResponseStr));
    ui->print(Progmem::getString(Progmem::uiNewLine));
  }
   
  // show disk stats
  if (cbSuccess)
  {
    ui->print(Progmem::getString(Progmem::uiNewLine));
    ui->print(Progmem::getString(Progmem::imgDiskStats));
    ui->print(Progmem::getString(Progmem::imgBadBlocks), cbTotalBadBlocks);
    ui->print(Progmem::getString(Progmem::imgBadTracks), cbUnreadableTracks);
    if (wdc->getParams()->DataVerifyMode != MODE_CRC_16BIT)
    {
      ui->print(Progmem::getString(Progmem::imgDataCorrected), cbTotalCorrectedErrors);  
    }    
    ui->print(Progmem::getString(Progmem::imgDataErrors), cbTotalDataErrors);
  }
  
  ui->print(Progmem::getString(Progmem::uiNewLine));
  ui->print(Progmem::getString(Progmem::uiContinue));
  ui->readKey("\r");  
  ui->print(Progmem::getString(Progmem::uiNewLine));
}

void CommandWriteImage()
{
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
  cbWriteImgOverrideParams = (key == 'Y');
  
  // if not, ask whether to work on the whole disk or partial image
  wdc->getParams()->PartialImage = false;
  wdc->getParams()->PartialImageStartCyl = 0;
  wdc->getParams()->PartialImageEndCyl = 0;
  
  if (!cbWriteImgOverrideParams && (wdc->getParams()->Cylinders > 1))
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
  cbWriteImgBadSectorMode = (key == 'B') ? 1 : 0;
  
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
    cbWriteImgDataErrorsMode = 0;
    break;
  case 'B':
    cbWriteImgDataErrorsMode = 1;
    break;
  case 'G':
    cbWriteImgDataErrorsMode = 2;
    break;
  }
  
  // XMODEM-1K
  bool useXMODEM1K = false;
  BYTE* testAlloc = new BYTE[1030];
  if (testAlloc)
  {
    delete[] testAlloc;
    
    ui->print(Progmem::getString(Progmem::uiNewLine));
    ui->print(Progmem::getString(Progmem::imgXmodem1k));
    key = toupper(ui->readKey("YN\e"));
    if (key == '\e')
    {
      ui->print(Progmem::getString(Progmem::uiNewLine));
      return;
    }
    ui->print(Progmem::getString(Progmem::uiEchoKey), key);
    useXMODEM1K = (key == 'Y');
  }   
  
  // make a backup of the current disk parameters struct
  WD42C22::DiskDriveParams backup;
  memcpy(&backup, wdc->getParams(), sizeof(WD42C22::DiskDriveParams));
  
  // seek to the beginning
  wdc->seekDrive(0, 0);
  
  ui->print("");
  ui->print(Progmem::getString(useXMODEM1K ? Progmem::imgXmodem1kPrefix : Progmem::imgXmodemPrefix));
  ui->print(Progmem::getString(Progmem::imgXmodemWaitSend));
  ui->setPrintDisabled(true);
  
  // reset values that are not modified by CbCleanup()
  cbProgmemResponseStr = 0;
  cbSuccess = false;
  cbInProgress = true;
  cbTotalDataErrors = 0;
  cbTotalBadBlocks = 0;
  cbUnreadableTracks = 0;
  
  // receive and write
  XModem modem(RX, TX, &CbWriteDisk, useXMODEM1K);
  modem.receive();
  // finished, later ask to restore previous drive settings if it processed fine
  const bool askRestore = cbWriteImgOverrideParams && !cbProcessingHeader && !cbProcessingDriveTable;
  CbCleanup();
  DumpSerialTransfer();
  wdc->selectDrive(false);
   
  ui->setPrintDisabled(false); 
  ui->print("");   
  ui->print(Progmem::getString(Progmem::uiDeleteLine));
  ui->print(Progmem::getString(Progmem::uiVT100ClearScreen));  
    
  ui->print(Progmem::getString(cbSuccess ? Progmem::imgXmodemXferEnd : Progmem::imgXmodemXferFail));
  ui->print(Progmem::getString(Progmem::uiNewLine));
  if (cbProgmemResponseStr)
  {
    ui->print(Progmem::getString(cbProgmemResponseStr));
    ui->print(Progmem::getString(Progmem::uiNewLine));
  }
   
  // show image and disk stats
  if (cbSuccess)
  {
    ui->print(Progmem::getString(Progmem::uiNewLine));
    ui->print(Progmem::getString(Progmem::imgImageStats));
    ui->print(Progmem::getString(Progmem::imgBadBlocks), cbTotalBadBlocks);
    ui->print(Progmem::getString(Progmem::imgBadTracks), cbUnreadableTracks); 
    ui->print(Progmem::getString(Progmem::imgDataErrors), cbTotalDataErrors);
    ui->print(Progmem::getString(Progmem::imgRunScan));    
  }
  
  ui->print(Progmem::getString(Progmem::uiNewLine));
  ui->print(Progmem::getString(Progmem::uiContinue));
  ui->readKey("\r");  
  ui->print(Progmem::getString(Progmem::uiNewLine));
  
  if (askRestore)
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

// XMODEM
int RX(int msDelay) 
{ 
  const DWORD start = millis();
  while ((millis()-start) < msDelay)
  { 
    if (Serial.available())
    {
      return (BYTE)Serial.read();
    }
  }

  return -1; 
}

void TX(const char *data, int size)
{  
  Serial.write((const BYTE*)data, size);
}

bool IsSerialTransfer()
{
  return cbInProgress;
}

// dump serial transfer if not successful
void DumpSerialTransfer()
{
  const BYTE CAN = 0x18;
  
  DWORD delayMs = millis() + 10;
  while (millis() < delayMs)
  {
    if (Serial.read() >= 0)
    {
      delayMs = millis() + 10;
    }
  }
  
  while (Serial.available() > 0)
  {
    Serial.read(); 
  }
  
  Serial.write(&CAN, sizeof(BYTE));
  Serial.write(&CAN, sizeof(BYTE));
  Serial.write(&CAN, sizeof(BYTE));
}

void CbCleanup()
{
  if (cbSectorsTable)
  {
    delete[] cbSectorsTable;
    cbSectorsTable = NULL;
  }
  
  cbInProgress              = false;
  cbProcessingHeader        = true;
  cbProcessingDriveTable    = false;
  cbCylinderSpecified       = false;
  cbHeadSpecified           = false;
  cbSptSpecified            = false;
  cbSecMapSpecified         = false;
  cbSecDataTypeSpecified    = false;
  
  cbLastPos                 = 0;
  cbCylinder                = 0;
  cbHead                    = 0;
  cbSpt                     = 0;
  cbCurrentSector           = 0;
  cbSectorsTableCount       = 0;
  cbSectorIdx               = 0;
  cbStartingSectorIdx       = (WORD)-1;
  cbSectorDataType          = 0;
  cbSecSizeBytes            = 0;
  
  memset(&cbParams, 0, sizeof(cbParams));
  
  wdc->sramFinishBufferAccess();
}

// read disk callback
bool CbReadDisk(DWORD packetNo, BYTE* data, WORD size)
{
  WORD packetIdx = 0;
  
  if (!data || ((size != 128) && (size != 1024)))
  {
    cbSuccess = false;
    cbProgmemResponseStr = Progmem::imgXmodemErrPacket;
    return false;
  }
  
  // fill the output buffer with ASCII EOF (end-of-file) padding so it's known where the transfer ended
  // as XMODEM sends fixed 128B or 1024B packets
  memset(data, 0x1A, size);
  
  // end of transfer
  if ((cbCylinder == wdc->getParams()->Cylinders) ||
      (wdc->getParams()->PartialImage && (cbCylinder-1 == wdc->getParams()->PartialImageEndCyl)))
  {
    return false;
  }
  
  for(;;)
  {
    // write WDI header and comment from SRAM to output file
    if (cbProcessingHeader)
    {       
      for (;;)
      {
        const BYTE sramByte = wdc->sramReadByteSequential();
        data[packetIdx++] = sramByte;
        
        if (sramByte == 0x1A)
        {
          cbProcessingHeader = false;
          cbProcessingDriveTable = true;
          cbLastPos = 0;
          
          wdc->sramFinishBufferAccess();          
          break;
        }
        
        CHECK_STREAM_END;
      }
      
      CHECK_STREAM_END;
    }
    
    // write DiskDriveParams to a total of 32 bytes; unused values pre-set to 0
    if (cbProcessingDriveTable)
    {
      while (cbLastPos < sizeof(cbParams))
      {
        data[packetIdx++] = cbParams[cbLastPos++];
        CHECK_STREAM_END;
      }
      
      cbLastPos = 0;
      cbProcessingDriveTable = false;
    }
    
    // current physical cylinder
    if (!cbCylinderSpecified)
    {
      cbCylinder = wdc->getPhysicalCylinder();
      
      if (cbLastPos == 0) // LSB
      {
        data[packetIdx++] = (BYTE)cbCylinder;
        cbLastPos++;
        CHECK_STREAM_END;
      }
      
      data[packetIdx++] = (BYTE)(cbCylinder >> 8); // MSB
      cbCylinderSpecified = true;
      cbLastPos = 0;
      CHECK_STREAM_END;
    }
    
    // head
    if (!cbHeadSpecified)
    {
      cbHead = wdc->getPhysicalHead();
      data[packetIdx++] = cbHead;
      cbHeadSpecified = true;
      CHECK_STREAM_END;
    }
    
    // sectors per track
    if (!cbSptSpecified)
    {
      if (!wdc->seekDrive(cbCylinder, cbHead))
      {
        cbSuccess = false;
        cbProgmemResponseStr = Progmem::uiFeSeek;
        return false;
      }
      
      if (cbSectorsTable)
      {
        delete[] cbSectorsTable;
        cbSectorsTable = NULL;
      }
      cbSectorsTableCount = 0;
      
      // get SDH byte, 5 attempts
      BYTE sdh;
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
            cbSuccess = false;
            cbProgmemResponseStr = wdc->getLastErrorMessage();
            return false;
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
        cbSpt = 0;
        cbSptSpecified = true;
        data[packetIdx++] = cbSpt;
        CHECK_STREAM_END;
        
        continue;
      }      
            
      // now calculate SPT
      bool dummy3;
      cbSectorsTable = CalculateSectorsPerTrack(sdh, cbSpt, cbSectorsTableCount, dummy3, dummy3, dummy3);
      if (!cbSectorsTable && !cbSectorsTableCount)
      {
        cbSuccess = false;
        cbProgmemResponseStr = Progmem::uiFeMemory;
        return false;
      }
      
      if (cbSpt)
      {
        // as the interleave table almost always never starts from the beginning, find the starting sector
        // but even the starting sector number might not start from 0
        WORD startingSector = 0;
        
        while (cbStartingSectorIdx == (WORD)-1) // undefined
        {
          bool found = false;
          
          for (WORD idx = 0; idx < cbSectorsTableCount; idx++)
          {                   
            if (cbSectorsTable[idx] == 0xFFFFFFFFUL) // undefined?
            {
              continue;
            }
            
            // logical sector number matching?
            if ((BYTE)(cbSectorsTable[idx] >> 16) == startingSector)
            {
              cbStartingSectorIdx = idx;
              cbSectorIdx = cbStartingSectorIdx;
              cbLastPos = 0; // use this as count how many were written in the map
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
            cbSpt = 0; // mark track as unreadable
            break;
          }
        }
      }
 
      cbSptSpecified = true;
      data[packetIdx++] = cbSpt;
      CHECK_STREAM_END;
    }
    
    // track contains no sectors?
    if (!cbSpt)
    {
      cbUnreadableTracks++;
      
      // re-specify
      cbCylinderSpecified = false;
      cbHeadSpecified = false;
      cbSptSpecified = false;
      cbSecMapSpecified = false;
      cbLastPos = 0;
      cbSectorIdx = 0;
      cbStartingSectorIdx = (WORD)-1;
      
      // seek to the next
      cbHead++;
      if (cbHead == wdc->getParams()->Heads)
      {
        cbHead = 0;
        cbCylinder++;
      }
      if ((cbCylinder == wdc->getParams()->Cylinders) ||
          (wdc->getParams()->PartialImage && (cbCylinder-1 == wdc->getParams()->PartialImageEndCyl)))
      {
        cbSuccess = true;
        cbProgmemResponseStr = 0;
        return true; // flush the buffer
      }
      
      if (!wdc->seekDrive(cbCylinder, cbHead))
      {
        cbSuccess = false;
        cbProgmemResponseStr = Progmem::uiFeSeek;
        return false;
      }
      
      continue; // transfer continues
    }
    
    // sector numbering map
    if (!cbSecMapSpecified)
    {
      static BYTE sectorMapPos = 0;      
      
      // now write the sector numbering map
      while ((cbLastPos < (WORD)cbSpt*4) && (cbSectorIdx < cbSectorsTableCount))
      {
        if (cbSectorsTable[cbSectorIdx] == 0xFFFFFFFFUL) // undefined?
        {
          cbSectorIdx++;
          sectorMapPos = 0;
          continue;
        }
        
        cbCurrentSector = (BYTE)(cbSectorsTable[cbSectorIdx] >> 16);
        
        const BYTE* sectorMap = (const BYTE*)(&cbSectorsTable[cbSectorIdx]); // access by bytes
        data[packetIdx++] = sectorMap[sectorMapPos++];
        
        cbLastPos++;
        if ((cbLastPos % 4) == 0)
        {
          cbSectorIdx++; // 4 bytes per each sector
          sectorMapPos = 0;
        }
        CHECK_STREAM_END;       
      }
      
      // sectors per track count not reached: do we still need to go from the beginning of the table?
      if ((cbSectorIdx == cbSectorsTableCount) && (cbLastPos < (WORD)cbSpt*4))
      {
        sectorMapPos = 0;
        bool found = false;
            
        while (cbCurrentSector && !found)
        {
          for (cbSectorIdx = 0; cbSectorIdx < cbSectorsTableCount; cbSectorIdx++)
          {
            if ((((BYTE)(cbSectorsTable[cbSectorIdx] >> 16)) == cbCurrentSector) &&
                 (cbSectorsTable[cbSectorIdx] != 0xFFFFFFFFUL))
            {
              found = true;
              break;
            }
          }
          
          if (!found)
          {
            cbCurrentSector++; // possible gap?
          }
        }            
        
        // found from the beginning, get the succeeding sector index
        if (found)
        {
          cbSectorIdx += 1;
          if (cbSectorIdx < cbSectorsTableCount)
          {
            continue; // valid
          }
        }

        // not found or out-of-bounds
        cbSectorIdx = 0;
        continue;
      }
      
      cbSectorIdx = cbStartingSectorIdx;
      cbLastPos = 0;
      sectorMapPos = 0;
      cbCurrentSector = 0;
      cbSecMapSpecified = true;
    }
    
    // now try to read    
    while ((cbLastPos < cbSpt) && (cbSectorIdx < cbSectorsTableCount))
    {     
      static WORD rwBufferPos = 0;
      
      if (!cbSecDataTypeSpecified) // determine data record type
      {
        rwBufferPos = 0;
        
        if (cbSectorsTable[cbSectorIdx] == 0xFFFFFFFFUL) // skip undefined
        {
          cbSectorIdx++;
          continue;
        }
        
        const BYTE sdh = (BYTE)(cbSectorsTable[cbSectorIdx] >> 24);        
        cbSecSizeBytes = wdc->getSectorSizeFromSDH(sdh);        
        
        const BYTE logicalSector = (BYTE)(cbSectorsTable[cbSectorIdx] >> 16);
        cbCurrentSector = logicalSector;
        const WORD logicalCylinder = (WORD)cbSectorsTable[cbSectorIdx];
        const BYTE logicalHead = sdh & 0xF;        
        
        wdc->readSector(logicalSector, cbSecSizeBytes, false, &logicalCylinder, &logicalHead);     
        if (wdc->getLastError())
        {
          if (wdc->getLastError() < 4) // WDC timeout, drive not ready, writefault
          {
            cbSuccess = false;
            cbProgmemResponseStr = wdc->getLastErrorMessage();
            return false;
          }
          
          else if (wdc->getLastError() == WDC_CORRECTED) // treat successful ECC correction as OK
          {
            cbSectorDataType = 1;
            cbTotalCorrectedErrors++;
          }
        
          else if (wdc->getLastError() == WDC_DATAERROR) // we have data, but likely faulty
          {
            cbSectorDataType = 2;
            cbTotalDataErrors++;
          }
          
          else // no data in buffer
          {
            cbSectorDataType = 0;
            cbTotalBadBlocks++;
          }
        }
        else
        {
          cbSectorDataType = 1; // valid data
        }
        
        // determine whether to compress the data
        if (cbSectorDataType)
        {
          wdc->sramBeginBufferAccess(false, 0);
          bool compressedData = true;
          BYTE lastData = wdc->sramReadByteSequential();
          
          for (WORD idx = 1; idx < cbSecSizeBytes; idx++)
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
            cbSectorDataType |= 0x80; //set bit 7
          }
          
          wdc->sramBeginBufferAccess(false, 0); // rewind SRAM buffer          
        }      
        
        data[packetIdx++] = cbSectorDataType; 
        cbSecDataTypeSpecified = true;
        CHECK_STREAM_END;
      }
                
      // data is ready
      switch(cbSectorDataType)
      {
      case 1:
      case 2:      
      {
        while (rwBufferPos != cbSecSizeBytes)
        {
          data[packetIdx++] = wdc->sramReadByteSequential();
          rwBufferPos++;
          CHECK_STREAM_END;
        }
        rwBufferPos = 0;
        wdc->sramFinishBufferAccess();
      }
      break;
      case 0x81:
      case 0x82:      
      {
        // compressed data (same byte repeated secSizeBytes)
        data[packetIdx++] = wdc->sramReadByteSequential();
        wdc->sramFinishBufferAccess();
        cbSectorDataType = 0; // go to next sector
        CHECK_STREAM_END;
      }
      break;
      }
      
      // next sector
      cbSectorIdx++;
      cbLastPos++;
      cbSecDataTypeSpecified = false;
    }    
    if ((cbSectorIdx == cbSectorsTableCount) && (cbLastPos < cbSpt))
    {
      bool found = false;
            
      while (cbCurrentSector && !found)
      {
        for (cbSectorIdx = 0; cbSectorIdx < cbSectorsTableCount; cbSectorIdx++)
        {
          if ((((BYTE)(cbSectorsTable[cbSectorIdx] >> 16)) == cbCurrentSector) &&
               (cbSectorsTable[cbSectorIdx] != 0xFFFFFFFFUL))
          {
            found = true;
            break;
          }
        }
        
        if (!found)
        {
          cbCurrentSector++;
        }
      }            
      
      if (found)
      {
        cbSectorIdx += 1;
        if (cbSectorIdx < cbSectorsTableCount)
        {
          continue;
        }
      }
      
      cbSectorIdx = 0;
      continue;
    }
       
    // end of track?
    cbSuccess = true;
    cbProgmemResponseStr = 0;

    // re-specify
    cbCylinderSpecified = false;
    cbHeadSpecified = false;
    cbSptSpecified = false;
    cbSecMapSpecified = false;
    cbSecDataTypeSpecified = false;
    cbLastPos = 0;
    cbSectorIdx = 0;
    cbCurrentSector = 0;
    cbStartingSectorIdx = (WORD)-1;   
    
    // and seek to next
    cbHead++;
    if (cbHead == wdc->getParams()->Heads)
    {
      cbHead = 0;
      cbCylinder++;
    }
    if ((cbCylinder == wdc->getParams()->Cylinders) ||    
        (wdc->getParams()->PartialImage && (cbCylinder-1 == wdc->getParams()->PartialImageEndCyl)))
    {
      cbSuccess = true;
      cbProgmemResponseStr = 0;
      return true; // flush the buffer
    }

    if (!wdc->seekDrive(cbCylinder, cbHead))
    {
      cbSuccess = false;
      cbProgmemResponseStr = Progmem::uiFeSeek;
      return false;
    } 
  }
  
  return false;   
}

// write disk callback
bool CbWriteDisk(DWORD packetNo, BYTE* data, WORD size)
{ 
  WORD packetIdx = 0;
  
  // what?
  if (!data || ((size != 128) && (size != 1024)))
  {
    cbSuccess = false;
    cbProgmemResponseStr = Progmem::imgXmodemErrPacket;
    return false;
  }
  
  // repeat until the packet is exhausted; returns true to ask for next, or returns false on error/transfer over
  for (;;)
  {
    // first step: check the header of variable length; skip its contents (needs to end with EOF)
    if (cbProcessingHeader)
    {     
      if (packetNo == 1)
      {
        // have the message ready in case we abort
        cbSuccess = false;
        cbProgmemResponseStr = Progmem::imgXmodemErrHeader;
        
        // verify it begins with "WDI " otherwise abort
        if (memcmp(data, "WDI ", 4) != 0)
        {
          return false;
        }
        packetIdx += 4;  
      }      
      
      // keep looking for ASCII EOF marking the end of header      
      while (cbProcessingHeader)
      {
        if (data[packetIdx] == 0x1A) // EOF
        {          
          cbProcessingHeader = false;
          cbProcessingDriveTable = true;
          cbProgmemResponseStr = 0; // header skipped OK as it contains text description only
          cbLastPos = 0;
        }
        packetIdx++;
        
        // always take care if we're not at the end of the 128B/1024B datastream 
        CHECK_STREAM_END;
      }
    }
    
    // copy 32 bytes of drive table into our array and then check it for validity
    if (cbProcessingDriveTable)
    {
      while (cbLastPos < sizeof(cbParams))
      {
        cbParams[cbLastPos++] = data[packetIdx++];
        CHECK_STREAM_END;
      }
      cbLastPos = 0;
      
      if (!CbVerifyParamsFromImage())
      {
        cbSuccess = false; // response text prepared
        return false;
      }
      
      // apply new parameters?
      if (cbWriteImgOverrideParams)
      {
        memcpy(wdc->getParams(), &cbParams[0], sizeof(WD42C22::DiskDriveParams));
        wdc->applyParams();
        
        if (wdc->getLastError())
        {
          cbSuccess = false;
          cbProgmemResponseStr = wdc->getLastErrorMessage();
          return false;
        }
      }
      
      cbProcessingDriveTable = false;
    }
    
    // current physical cylinder
    if (!cbCylinderSpecified)
    {     
      if (cbSectorsTable) // next?
      {
        delete[] cbSectorsTable;
        cbSectorsTable = NULL;
      }
      
      if (cbLastPos == 0) // LSB
      {
        cbCylinder = data[packetIdx++];
        cbLastPos++;
        CHECK_STREAM_END;
      }
      
      // MSB 0..7
      BYTE byte = data[packetIdx++];
      if (byte == 0x1A) // end-of-file? transfer over
      {
        cbSuccess = true;
        cbProgmemResponseStr = 0;
        return false;
      }
      cbCylinder |= (WORD)(byte << 8);
      
      // check if within bounds
      if (cbCylinder >= wdc->getParams()->Cylinders)
      {
        cbSuccess = false;
        cbProgmemResponseStr = Progmem::imgXmodemErrCyls;
        return false;
      }
      
      cbCylinderSpecified = true;
      cbLastPos = 0;
      CHECK_STREAM_END;
    }
    
    // head
    if (!cbHeadSpecified)
    {
      cbHead = data[packetIdx++];
      if (cbHead >= wdc->getParams()->Heads)
      {
        cbSuccess = false;
        cbProgmemResponseStr = Progmem::imgXmodemErrHeads;
        return false;
      }
      
      cbHeadSpecified = true;
      CHECK_STREAM_END;
    }
    
    // sectors per track
    if (!cbSptSpecified)
    {
      cbSpt = data[packetIdx++];
      cbSptSpecified = true;
      CHECK_STREAM_END;
    }
    
    // no sectors in track? advance
    if (!cbSpt)
    {
      cbUnreadableTracks++;      
      cbCylinderSpecified = false;
      cbHeadSpecified = false;
      cbSptSpecified = false;
      cbLastPos = 0;
      continue;
    }
    
    // write partial image: skip over data?
    const bool partialImageSkipData = wdc->getParams()->PartialImage && 
                                      ((cbCylinder < wdc->getParams()->PartialImageStartCyl) || (cbCylinder > wdc->getParams()->PartialImageEndCyl));
    
    // allocate sectors table and format
    if (!cbSecMapSpecified)
    {
      if (!cbSectorsTable)
      {
        cbSectorsTable = new DWORD[cbSpt];
        if (!cbSectorsTable)
        {
          cbSuccess = false;
          cbProgmemResponseStr = Progmem::uiFeMemory;
          return false;
        }  
      }

      // address by bytes
      BYTE* sectorsTable = (BYTE*)cbSectorsTable;
      while (cbLastPos < (WORD)cbSpt*4) // 4 bytes per each sector
      {
        sectorsTable[cbLastPos++] = data[packetIdx++];
        CHECK_STREAM_END;
      }
      
      cbLastPos = 0;
      cbSectorIdx = 0;
      cbSecMapSpecified = true;
      
      const BYTE sdh = (BYTE)(cbSectorsTable[0] >> 24);
      const WORD logicalCylinder = (WORD)cbSectorsTable[0];
      const BYTE logicalHead = sdh & 0xF;        
      cbSecSizeBytes = wdc->getSectorSizeFromSDH(sdh);
      
      // write partial image: skip over
      if (partialImageSkipData)
      {
        continue;
      }
           
      // since we need to format, and set gaps, make sure there are no variable size sectors,
      // and that the logical cylinder and head numbers do not differ between each other.
      // -> the Format Track command of the WD42C22 has no provision of customizing these between each,
      // as the value is taken from a task register, for the whole track.
      // ...otherwise we would have to call writeID to overwrite each sector ID and risk losing data,
      // as this command requires a precise byte offset where to write the changes...      
      
      // inspect the first logical sector and verify the rest
      wdc->sramBeginBufferAccess(true, 0);
      wdc->sramWriteByteSequential(0);
      wdc->sramWriteByteSequential((BYTE)(cbSectorsTable[0] >> 16));
      
      for (WORD idx = 1; idx < cbSpt; idx++)
      {
        const BYTE thisSdh = (BYTE)(cbSectorsTable[idx] >> 24);        
        if (wdc->getSectorSizeFromSDH(thisSdh) != cbSecSizeBytes)
        {
          cbSuccess = false;
          cbProgmemResponseStr = Progmem::imgXmodemErrVar1;
          return false;
        }
        
        const WORD thisCylinder = (WORD)cbSectorsTable[idx];
        const BYTE thisHead = thisSdh & 0xF;
        if ((thisHead != logicalHead) || (thisCylinder != logicalCylinder))
        {
          cbSuccess = false;
          cbProgmemResponseStr = Progmem::imgXmodemErrVar2;
          return false;
        }
        
        // create format interleave table, set good sectors and later in the datastream, find out which ones are bad
        wdc->sramWriteByteSequential(0);
        wdc->sramWriteByteSequential((BYTE)(cbSectorsTable[idx] >> 16));
      }
      wdc->sramFinishBufferAccess();
      
      // prepare for writing, seek the drive
      if (!wdc->seekDrive(cbCylinder, cbHead))
      {
        cbSuccess = false;
        cbProgmemResponseStr = Progmem::uiFeSeek;
        return false;
      }
      
      // format
      wdc->formatTrack(cbSpt, cbSecSizeBytes, &logicalCylinder, &logicalHead);
      
      // formatTrack can only fail with WDC timeout, drive not ready or write fault
      if (wdc->getLastError())
      {
        cbSuccess = false;
        cbProgmemResponseStr = wdc->getLastErrorMessage();
        return;
      }      
    }
    
    // determine what to write
    if (!cbSecDataTypeSpecified)
    {
      cbSectorDataType = data[packetIdx++];
      if ((cbSectorDataType & 0x7F) > 2)
      {
        cbSuccess = false;
        cbProgmemResponseStr = Progmem::imgXmodemErrSecTyp;
        return false;
      }
      
      cbSecDataTypeSpecified = true;
      CHECK_STREAM_END;
    }
    
    // so far so good
    cbSuccess = true;
    cbProgmemResponseStr = 0;
    
    // write, depending on type
    if (!partialImageSkipData && (cbSectorIdx < cbSpt))
    {     
      const BYTE sdh = (BYTE)(cbSectorsTable[cbSectorIdx] >> 24);
      const WORD logicalCylinder = (WORD)cbSectorsTable[cbSectorIdx];
      const BYTE logicalHead = sdh & 0xF;
      const BYTE logicalSector = (BYTE)(cbSectorsTable[cbSectorIdx] >> 16);
      
      // unreadable sector
      if (cbSectorDataType == 0)
      {
        // already formatted empty...
        if (cbWriteImgBadSectorMode == 1) // also flag as bad?
        {
          wdc->setBadSector(logicalSector, &logicalCylinder, &logicalHead);
        }
        
        // continue with the next
        cbTotalBadBlocks++;
        cbSectorIdx++;
        if (cbSectorIdx < cbSpt)
        {
          cbSecDataTypeSpecified = false;
          continue;
        }
      }
      
      // data follows
      else
      {
        bool doNotWrite = false; 
        bool formatBad = false;  
        
        // contains CRC/ECC error?
        if ((cbSectorDataType & 0x7F) == 2)
        { 
          if (cbWriteImgDataErrorsMode == 0)
          {
            doNotWrite = true; // just keep formatted empty
          }
          else if (cbWriteImgDataErrorsMode == 1)
          {
            doNotWrite = true;
            formatBad = true; // do not write and set sector ID as bad
          }
        }
        
        // initialize SRAM buffer write
        if ((cbLastPos == 0) && !doNotWrite && !formatBad)
        {
          wdc->sramBeginBufferAccess(true, 0);
        }
        
        // all data are of the same byte - compressed
        if (cbSectorDataType & 0x80)
        {
          BYTE compressed = data[packetIdx++];
          cbLastPos++;
          
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
            for (WORD index = 0; index < cbSecSizeBytes; index++)
            {
              wdc->sramWriteByteSequential(compressed);
            }            
          }
          
          wdc->sramFinishBufferAccess();          
        }
        
        // normal data
        else
        {
          while (cbLastPos != cbSecSizeBytes)
          {
            BYTE byte = data[packetIdx++];
            if (!doNotWrite)
            {
              wdc->sramWriteByteSequential(byte);
            }
            
            cbLastPos++;            
            CHECK_STREAM_END;
          }
          wdc->sramFinishBufferAccess();
        }
          
        if (!doNotWrite)  
        {
          // write
          wdc->writeSector(logicalSector, cbSecSizeBytes, &logicalCylinder, &logicalHead);     
          if (wdc->getLastError())
          {
            if (wdc->getLastError() < 4) // WDC timeout, drive not ready, writefault
            {
              cbSuccess = false;
              cbProgmemResponseStr = wdc->getLastErrorMessage();
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
        // we can't increment this at the doNotWrite setter above;
        // as multiple reentrancies due to CHECK_STREAM_END would cause false counts
        if ((cbSectorDataType & 0x7F) == 2)
        {
          cbTotalDataErrors++;
        }
          
        // next sector 
        cbLastPos = 0;        
        cbSectorIdx++;
        if (cbSectorIdx < cbSpt)
        {
          cbSecDataTypeSpecified = false;
        }
        
        CHECK_STREAM_END;
        continue;          
      }      
    }
    
    else if (partialImageSkipData) // just skip over data packet
    {
      if (cbSectorIdx < cbSpt)
      {
        if (cbSectorDataType == 0) // no data would follow
        {
          cbSectorIdx++;
          if (cbSectorIdx < cbSpt)
          {
            cbSecDataTypeSpecified = false;
            continue;
          }
        }
        
        else
        {
          if (cbSectorDataType & 0x80) // 1 byte follows
          {
            packetIdx++;
            cbLastPos++;
          }
          else // sector data follows
          {
            while (cbLastPos != cbSecSizeBytes)
            {
              packetIdx++;              
              cbLastPos++;            
              CHECK_STREAM_END;
            }
          }
          
          // next sector 
          cbLastPos = 0;        
          cbSectorIdx++;
          if (cbSectorIdx < cbSpt)
          {
            cbSecDataTypeSpecified = false;
          }
          
          CHECK_STREAM_END;
          continue;
        }
      }
    }
    
    // specify next track data field
    cbSectorIdx = 0;
    cbLastPos = 0;
    cbCylinderSpecified = false;
    cbHeadSpecified = false;
    cbSptSpecified = false;
    cbSecMapSpecified = false;
    cbSecDataTypeSpecified = false;
  }
  
  // doesn't reach here
  return false;
}

bool CbVerifyParamsFromImage()
{ 
  // assume error
  BYTE backup = cbProgmemResponseStr;
  cbProgmemResponseStr = Progmem::imgXmodemErrParams;
  
  // access loaded array as disk drive parameters POD
  WD42C22::DiskDriveParams* params = (WD42C22::DiskDriveParams*)(&cbParams[0]);
  
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
  if (!cbWriteImgOverrideParams && (params->UseRLL != wdc->getParams()->UseRLL))
  {
    cbProgmemResponseStr = Progmem::imgXmodemErrMFMRLL; // inform about mismatch
    return false;
  }
  
  // check partial image bounds
  if (wdc->getParams()->PartialImage)
  {
    cbProgmemResponseStr = Progmem::imgXmodemErrPart;
    
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
  
  cbProgmemResponseStr = backup; // alles in Ordnung
  return true;
}