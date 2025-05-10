// Winchesterduino FATFS overrides

#include "..\..\config.h" // we

#include "diskio.h"

DSTATUS disk_status(BYTE pdrv)
{ 
  // unused
  return 0;
}

DSTATUS disk_initialize(BYTE pdrv)
{ 
  // unused
  return 0;
}

DRESULT disk_read(BYTE pdrv, BYTE *buf, DWORD sec, UINT count)
{ 
  // FATFS operates in single sectors
  if ((count != 1) || (sec > DOSGetTotalSectorCount()))
  {
    return RES_PARERR; 
  }
  
  WORD cyl;
  BYTE head;
  BYTE sector;
  DOSConvertLogicalSectorToCHS(sec, cyl, head, sector);
  
  wdc->seekDrive(cyl, head);
  wdc->readSector(sector, DOSGetSectorSize());
  
  // allow ECC
  if (wdc->getLastError() && (wdc->getLastError() != WDC_CORRECTED))
  {
    return RES_ERROR;
  }
  
  wdc->sramBeginBufferAccess(false, 0);
  for (WORD idx = 0; idx < DOSGetSectorSize(); idx++)
  {
    buf[idx] = wdc->sramReadByteSequential();
  }
  wdc->sramFinishBufferAccess();

  return RES_OK;
}


// analog to the one above
DRESULT disk_write(BYTE pdrv, BYTE *buf, DWORD sec, UINT count)
{ 
  if ((count != 1) || (sec > DOSGetTotalSectorCount()))
  {
    return RES_PARERR;
  }
  
  WORD cyl;
  BYTE head;
  BYTE sector;
  DOSConvertLogicalSectorToCHS(sec, cyl, head, sector);
  
  wdc->sramBeginBufferAccess(true, 0);
  for (WORD idx = 0; idx < DOSGetSectorSize(); idx++)
  {
    wdc->sramWriteByteSequential(buf[idx]);
  }
  wdc->sramFinishBufferAccess();
  
  wdc->seekDrive(cyl, head);
  wdc->writeSector(sector, DOSGetSectorSize());
    
  // write
  if (wdc->getLastError() && (wdc->getLastError() != WDC_CORRECTED))
  {
    return RES_ERROR;
  }
  
  return RES_OK;
}

DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void* buff)
{
  DRESULT res = RES_ERROR;
  
  switch (cmd)
  {
  case CTRL_SYNC: 
    res = RES_OK; 
    break;

  case GET_SECTOR_COUNT:
    *((DWORD *) buff) = (DWORD)DOSGetTotalSectorCount();
    res = RES_OK;
    break;

  case GET_SECTOR_SIZE:
    *((DWORD *) buff) = (DWORD)DOSGetSectorSize();
    break;

  case GET_BLOCK_SIZE:
    *((DWORD *) buff) = 1;
    break;
  }
  
  return res;
}
