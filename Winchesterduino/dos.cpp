// Winchesterduino (c) 2025 J. Bogin, http://boginjr.com
// DOS filesystem viewer

#include "config.h"

// verify FAT operation macros
#define FAT_EXECUTE(fn)       if (DOSResult((fn)) != FR_OK) return;
#define FAT_EXECUTE_0(fn)     if (DOSResult((fn)) != FR_OK) return 0;
#define FAT_EXECUTE_DIR(fn)   if (DOSResult((fn)) != FR_OK) { f_closedir(&dir); return; }
#define FAT_EXECUTE_FILE(fn)  if (DOSResult((fn)) != FR_OK) { f_close(&file); return; }

FATFS fat                = {0};
FIL file                 = {0};
DIR dir                  = {0};

// path buffers; current and with filename
BYTE path[MAX_PATH+1]    = {0};
BYTE addPath[MAX_PATH+1] = {0};

BYTE sectorsPerTrack     = 0;   // uniform for all
WORD sectorSizeBytes     = 0;   // ditto + shall be max. 512 bytes
BYTE fsErrorMessage      = 0;   // Progmem index

WORD DOSGetSectorSize()
{
  return sectorSizeBytes;
}

DWORD DOSGetTotalSectorCount()
{
   return (DWORD)wdc->getParams()->Cylinders * wdc->getParams()->Heads * sectorsPerTrack;
}

void DOSConvertLogicalSectorToCHS(const DWORD& logical, WORD& cylinder, BYTE& head, BYTE& sector)
{ 
  cylinder = (logical / sectorsPerTrack) / wdc->getParams()->Heads;
  head = (logical / sectorsPerTrack) % wdc->getParams()->Heads;
  sector = (logical % sectorsPerTrack) + 1;
}

FRESULT DOSResult(FRESULT result)
{ 
  switch (result)
  {
  case FR_OK:
    fsErrorMessage = 0;
    break;
  case FR_NO_FILE:
    fsErrorMessage = Progmem::dosFileNotFound;
    break;
  case FR_NO_PATH:
    fsErrorMessage = Progmem::dosPathNotFound;
    break;
  case FR_INVALID_NAME:
    fsErrorMessage = Progmem::dosInvalidName;
    break;
  case FR_DENIED:
    fsErrorMessage = Progmem::dosDirectoryFull;
    break;
  case FR_EXIST:
    fsErrorMessage = Progmem::dosFileExists;
    break;
  case FR_INVALID_OBJECT:
    fsErrorMessage = Progmem::dosFsError;
    break;
  case FR_NOT_ENABLED:
    fsErrorMessage = Progmem::dosFsMountError;
    break;
  case FR_NO_FILESYSTEM:
    fsErrorMessage = Progmem::dosFsMountError;
    break;  
  case FR_NOT_ENOUGH_CORE:
    fsErrorMessage = Progmem::uiFeMemory;
    break;
  case FR_INVALID_PARAMETER:
    fsErrorMessage = Progmem::dosFsError;
    break;
  default:
    fsErrorMessage = Progmem::dosDiskError;
    break;
  }
   
  if (fsErrorMessage)
  {
    ui->print(Progmem::getString(fsErrorMessage));
    
    // disk error?
    if ((fsErrorMessage == Progmem::dosDiskError) && wdc->getLastErrorMessage())
    {
      ui->print(Progmem::getString(wdc->getLastErrorMessage()));
      ui->print(Progmem::getString(Progmem::uiNewLine));
    }
    
    ui->print(Progmem::getString(Progmem::uiNewLine));
  }
  
  return result;
}

bool DOSInitialize()
{
  // look at track 0
  WORD dummy;
  BYTE dummy2;
  BYTE sdh;
  
  wdc->seekDrive(0, 0);
  wdc->scanID(dummy, dummy2, sdh);
  if (wdc->getLastError())
  {
    ui->print(Progmem::getString(Progmem::analyzeNoSectors));
    ui->print(Progmem::getString(Progmem::uiNewLine));
    return false;
  }
  
  sectorSizeBytes = wdc->getSectorSizeFromSDH(sdh);  
  if (!sectorSizeBytes || (sectorSizeBytes > 512))
  {
    ui->print(Progmem::getString(Progmem::dosInvalidSsize), sectorSizeBytes);
    ui->print(Progmem::getString(Progmem::uiNewLine));
    return false;
  }
  
  bool weirdGeometry = false; // :-)
  DWORD* result = CalculateSectorsPerTrack(sdh, sectorsPerTrack, dummy,
                                           weirdGeometry, weirdGeometry, weirdGeometry);
  if (!result || !sectorsPerTrack || weirdGeometry)
  {
    if (result)
    {
      // sectors table
      delete[] result;
    }
    
    ui->print(Progmem::getString(Progmem::analyzeNoSectors));
    ui->print(Progmem::getString(Progmem::uiNewLine));
    return false;
  }  
  delete[] result;
  
  // try to mount first partition
  FAT_EXECUTE_0(f_mount(&fat, "0:", 1));

  return true;
}

void DOSFinish()
{
  // unmount
  f_closedir(&dir);
  f_unmount("0:");
}

void DOSMkdir(const BYTE* dirName)
{ 
  // overflow checks in CD command, as absolute paths in names are not allowed  
  memset(addPath, 0, sizeof(addPath));
  strcat(addPath, path);
  strcat(addPath, dirName);
  
  FAT_EXECUTE(f_mkdir(addPath));
}

// remove empty directory
void DOSRmdir(const BYTE* dirName)
{  
  memset(addPath, 0, sizeof(addPath));
  strcat(addPath, path);
  strcat(addPath, dirName);
  
  FAT_EXECUTE(f_rmdir(addPath));
  ui->print(Progmem::getString(Progmem::uiNewLine));
}

void DOSDel(const BYTE* fileName)
{ 
  memset(addPath, 0, sizeof(addPath));
  strcat(addPath, path);
  strcat(addPath, fileName);
  
  FAT_EXECUTE(f_unlink(addPath));
  ui->print(Progmem::getString(Progmem::uiNewLine));
}

void DOSType(const BYTE* fileName)
{  
  memset(addPath, 0, sizeof(addPath));
  strcat(addPath, path);
  strcat(addPath, fileName);
  
  WORD count = 1;
  FAT_EXECUTE(f_open(&file, addPath, FA_READ));
  
  while (count)
  {
    // to ui buffer, 0s skipped
    FAT_EXECUTE_FILE(f_read(&file, ui->getPrintBuffer(), MAX_CHARS, &count));
    if (count)
    {
      ui->setPrintLength(count);
      ui->print(ui->getPrintBuffer());  
    }    
  }
  
  FAT_EXECUTE(f_close(&file));
  ui->print(Progmem::getString(Progmem::uiNewLine));
}

void DOSTypeInto(const BYTE* fileName)
{ 
  memset(addPath, 0, sizeof(addPath));
  strcat(addPath, path);
  strcat(addPath, fileName);
  
  FAT_EXECUTE(f_open(&file, addPath, FA_WRITE | FA_OPEN_APPEND));
  
  // wait for ENTER keypress
  ui->print(Progmem::getString(Progmem::dosTypeInto));
  ui->print(Progmem::getString(Progmem::uiContinue));
  ui->readKey("\r");
  ui->print("");
  
  bool emptyLine = false;  
  for (;;)
  {
    // all valid keys allowed
    const BYTE* promptBuffer = ui->prompt();
    WORD length = strlen(promptBuffer);
    
    // quit?
    if (!length && emptyLine)
    {
      break;
    }    
    emptyLine = length == 0;
    
    // write line   
    WORD dummy;
    FAT_EXECUTE_FILE(f_write(&file, promptBuffer, length, &dummy));
    FAT_EXECUTE_FILE(f_write(&file, Progmem::getString(Progmem::uiNewLine), 2, &dummy));

    ui->print(Progmem::getString(Progmem::uiNewLine));
  }
  
  FAT_EXECUTE(f_close(&file));
  ui->print(Progmem::getString(Progmem::uiNewLine));
}

bool DOSChdir()
{  
  FAT_EXECUTE_0(f_opendir(&dir, path));
  FAT_EXECUTE_0(f_closedir(&dir));
  return true;
}

// list contents of current working directory
void DOSDir()
{ 
  BYTE* printBuffer = ui->getPrintBuffer();
  BYTE key = 0;
  WORD entriesCount = 0;
  FAT_EXECUTE(f_opendir(&dir, path));
  
  while(true)
  {
    FILINFO info = {0};
    FAT_EXECUTE_DIR(f_readdir(&dir, &info));
    
    // empty entry
    BYTE* name = info.fname;   
    if (!name || !strlen(name))
    {
      break;
    }
    
    printBuffer[0] = 0;
    
    // directory?
    if (info.fattrib & AM_DIR)
    {
      strncat(printBuffer, Progmem::getString(Progmem::dosDirectory), MAX_CHARS);
    }
    
    // file, print size
    else
    {     
      snprintf(printBuffer, MAX_CHARS, Progmem::getString(Progmem::dosBytesFormat), info.fsize);
      strncat(printBuffer, Progmem::getString(Progmem::uiBytes), MAX_CHARS);
      strncat(printBuffer, " ", MAX_CHARS);
    }
    
    // attributes readonly, archive, hidden, system
    strncat(printBuffer, info.fattrib & AM_RDO ? "R" : "-", MAX_CHARS);
    strncat(printBuffer, info.fattrib & AM_ARC ? "A" : "-", MAX_CHARS);
    strncat(printBuffer, info.fattrib & AM_HID ? "H" : "-", MAX_CHARS);
    strncat(printBuffer, info.fattrib & AM_SYS ? "S" : "-", MAX_CHARS);
        
    // name
    strncat(printBuffer, " ", MAX_CHARS);
    strncat(printBuffer, name, MAX_CHARS);
        
    // next entry
    strncat(printBuffer, Progmem::getString(Progmem::uiNewLine), MAX_CHARS);
    entriesCount++;
    
    // print line
    ui->print(printBuffer);
  }
  
  // end of listing
  FAT_EXECUTE(f_closedir(&dir));
  if (!entriesCount)
  {
    ui->print(Progmem::getString(Progmem::dosDirectoryEmpty));
    ui->print(Progmem::getString(Progmem::uiNewLine));
  }
  
  // show free space
  FATFS* dummy;
  DWORD freeClusters = 0;
  FAT_EXECUTE(f_getfree("0:", &freeClusters, &dummy));
  
  snprintf(printBuffer, MAX_CHARS, Progmem::getString(Progmem::dosBytesFormat), (freeClusters * fat.csize * sectorSizeBytes));
  strncat(printBuffer, Progmem::getString(Progmem::dosBytesFree), MAX_CHARS);
  ui->print(printBuffer);  
  ui->print(Progmem::getString(Progmem::uiNewLine));
}

// uppercase string
void ToUpper(BYTE* str)
{
  if (!str)
  {
    return;
  }
  
  for (WORD index = 0; index < strlen(str); index++)
  {
    str[index] = toupper(str[index]);
  }
}

// check for invalid characters
bool VerifySuppliedPath(const BYTE* pathToCheck)
{
  return !strpbrk(pathToCheck, Progmem::getString(Progmem::dosForbiddenChars));
}

void CommandDos()
{
  if (!DOSInitialize())
  {
    return;
  }
  
  // print partition size
  const WORD partitionSize = (((fat.n_fatent-2) * fat.csize) * (DWORD)sectorSizeBytes) / 1048576UL;
  ui->print(Progmem::getString(Progmem::uiVT100ClearScreen));
  ui->print(Progmem::getString(Progmem::dosMounted), partitionSize);
  ui->print(Progmem::getString(Progmem::dosCommands));
  ui->print(Progmem::getString(Progmem::dosCommandsList));
  
  // commands loop
  while(true)
  {
    wdc->selectDrive(false); // drive not needed at the moment
    
    // commands - max length 8
    // arguments - only 8.3 file name allowed for all, with a dot and a terminating \0
    BYTE command[8 + 1] = {0};
    BYTE arguments[12 + 1] = {0};
    
    ui->print("C:\\");
    if (strlen(path))
    {
      // don't display the trailing backslash
      BYTE* backslash = &path[strlen(path)-1];
      *backslash = 0;
      ui->print(path);
      *backslash = '\\';      
    }
    ui->print(">");
    
    // wait for command (8+12 characters and a space)
    sscanf(ui->prompt(21), "%8s %12s", command, arguments);
    ToUpper(command);
    ToUpper(arguments);
        
    // empty command
    if (!strlen(command))
    {
      ui->print("\r");
      continue;
    }
    
    ui->print(Progmem::getString(Progmem::uiNewLine));
    
    // EXIT
    if (strcmp_P(command, PSTR("EXIT")) == 0)
    {
      DOSFinish(); // unmount
      return;
    }
    
    // CD
    else if ((strcmp_P(command, PSTR("CD")) == 0) ||
             (strcmp_P(command, PSTR("CD.")) == 0) ||
             (strcmp_P(command, PSTR("CD..")) == 0) ||
             (strcmp_P(command, PSTR("CD\\")) == 0))
    {     
      // CD\ and CD \ <- go to the root directory
      if ((strcmp_P(command, PSTR("CD\\")) == 0) ||
         ((strcmp_P(command, PSTR("CD")) == 0) && (strcmp(arguments, "\\") == 0)))
      {
        path[0] = 0;
        
        ui->print(Progmem::getString(Progmem::uiNewLine));
        continue;
      }
      
      // CD.. and CD .. <- go one level up
      else if ((strcmp_P(command, PSTR("CD..")) == 0) ||
              ((strcmp_P(command, PSTR("CD")) == 0) && (strcmp(arguments, "..") == 0)))
      {
        if (strlen(path) > 1)
        {
          // cancel out ending backslash and find the second to last          
          path[strlen(path)-1] = 0;         
          BYTE* prevBackslash = strrchr(path, '\\');
          
          // go back to root directory
          if (!prevBackslash)
          {
            path[0] = 0;
          }
          else
          {
            *(prevBackslash+1) = 0;
          }
        }
                
        ui->print(Progmem::getString(Progmem::uiNewLine));
        continue;        
      }
      
      // CD (empty argument), CD. and CD . <- display current directory on new line
      else if (((strcmp_P(command, PSTR("CD")) == 0) && (!strlen(arguments))) ||
                (strcmp_P(command, PSTR("CD.")) == 0) ||
               ((strcmp_P(command, PSTR("CD")) == 0) && (strcmp(arguments, ".") == 0)))
      {
        ui->print("C:\\");
        if (strlen(path))
        {
          BYTE* backslash = &path[strlen(path)-1];
          *backslash = 0;
          ui->print(path);
          *backslash = '\\';
        }
        ui->print(Progmem::getString(Progmem::uiNewLine2x));
        continue;
      }
           
      // verify path
      if (!VerifySuppliedPath(arguments))
      {
        ui->print(Progmem::getString(Progmem::dosInvalidDirName));
        ui->print(Progmem::getString(Progmem::uiNewLine2x));
        continue;
      }
      
      // CD directory, verify we're not too nested deeply
      // (current path + new directory + backslash after + 8.3 filename with dot)
      const BYTE newDirLen = strlen(arguments);      
      if ((strlen(path) + newDirLen + 12) > MAX_PATH)
      {
        ui->print(Progmem::getString(Progmem::dosMaxPath));
        ui->print(Progmem::getString(Progmem::uiNewLine2x));
        continue;
      }
      
      // try if current path works
      if (!DOSChdir())
      {
        continue;
      }
                 
      //append path and backslash, try changing directory
      strncat(path, arguments, MAX_PATH);
      strncat(path, "\\", MAX_PATH);
            
      if (!DOSChdir())
      {
        // failed, shorten the path
        path[strlen(path)-1] = 0;         
        BYTE* prevBackslash = strrchr(path, '\\');
        
        // go back to root directory
        if (!prevBackslash)
        {
          path[0] = 0;
        }
        else
        {
          *(prevBackslash+1) = 0;
        }
      }
      else
      {
        ui->print(Progmem::getString(Progmem::uiNewLine));
      }

      continue;
    }
    
    // DIR
    else if (strcmp_P(command, PSTR("DIR")) == 0)
    {
      
      // provided 1 directory from our working path
      if (strlen(arguments))
      {
        if (!VerifySuppliedPath(arguments))
        {
          ui->print(Progmem::getString(Progmem::dosInvalidDirName));
          ui->print(Progmem::getString(Progmem::uiNewLine2x));
          continue;
        }
        
        // verify we're not too nested deeply (1 char extra for backslash)
        const BYTE newDirLen = strlen(arguments);      
        if ((strlen(path) + newDirLen + 1) > MAX_PATH)
        {
          ui->print(Progmem::getString(Progmem::dosMaxPath));
          ui->print(Progmem::getString(Progmem::uiNewLine2x));
          continue;
        }
        
        // try if current path works        
        if (!DOSChdir())
        {
          continue;
        }
        
        // change working directory for a while (similar to CD above)
        strncat(path, arguments, MAX_PATH);
        strncat(path, "\\", MAX_PATH);
        
        if (DOSChdir())
        {
          ui->print("");
          DOSDir();
        }
        
        // get the path back again to original
        path[strlen(path)-1] = 0;         
        BYTE* prevBackslash = strrchr(path, '\\');
        
        if (!prevBackslash)
        {
          path[0] = 0;
        }
        else
        {
          *(prevBackslash+1) = 0;
        }
        
        continue;
      }
      
      // show current directory contents
      ui->print("");      
      DOSDir();
      continue;
    }
    
    // MKDIR, RMDIR
    else if ((strcmp_P(command, PSTR("MKDIR")) == 0) ||
             (strcmp_P(command, PSTR("RMDIR")) == 0))
    {
      
      // 1 directory name
      if (strlen(arguments))
      {
        if (!VerifySuppliedPath(arguments))
        {
          ui->print(Progmem::getString(Progmem::dosInvalidDirName));
          ui->print(Progmem::getString(Progmem::uiNewLine2x));
          continue;
        }
              
        // remove directory
        if (strcmp_P(command, PSTR("RMDIR")) == 0)
        {
          DOSRmdir(arguments);  
        }
        
        // make directory
        else
        {
          DOSMkdir(arguments);
        }        

        continue;
      }
      
      ui->print(Progmem::getString(Progmem::dosInvalidDirName));
      ui->print(Progmem::getString(Progmem::uiNewLine2x));
      continue;
    }
    
    // DEL, TYPE
    else if ((strcmp_P(command, PSTR("DEL")) == 0) ||
             (strcmp_P(command, PSTR("TYPE")) == 0))
    {
      
      // 1 file name
      if (strlen(arguments))
      {
        if (!VerifySuppliedPath(arguments))
        {
          ui->print(Progmem::getString(Progmem::dosInvalidName));
          ui->print(Progmem::getString(Progmem::uiNewLine2x));
          continue;
        }
        
        // DEL
        if (strcmp_P(command, PSTR("DEL")) == 0)
        {
          DOSDel(arguments);
        }
        
        // TYPE
        else
        {                     
          DOSType(arguments);            
        }          
        
        continue;
      }
      
      ui->print(Progmem::getString(Progmem::dosInvalidName));
      ui->print(Progmem::getString(Progmem::uiNewLine2x));
      continue;
    }
    
    // TYPEINTO
    else if (strcmp_P(command, PSTR("TYPEINTO")) == 0)
    {
            
      // 1 file name
      if (strlen(arguments))
      {
        if (!VerifySuppliedPath(arguments))
        {
          ui->print(Progmem::getString(Progmem::dosInvalidName));
          ui->print(Progmem::getString(Progmem::uiNewLine2x));
          continue;
        }
        
        DOSTypeInto(arguments);
        continue;
      }
      
      ui->print(Progmem::getString(Progmem::dosInvalidName));
      ui->print(Progmem::getString(Progmem::uiNewLine2x));
      continue;
    }
    
    // prompt bubbled through - unrecognized command
    ui->print(Progmem::getString(Progmem::dosInvalidCommand));
    ui->print(Progmem::getString(Progmem::uiNewLine2x));
  }
}

  