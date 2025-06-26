// Winchesterduino (c) 2025 J. Bogin, http://boginjr.com
// WD42C22 Winchester Disk controller interface

#pragma once
#include "config.h"

// compile-time clock delay (Mega2560 clocked at 16MHz)
#define DELAY_CYCLES(n) __builtin_avr_delay_cycles(n);
#define DELAY_US(n)     DELAY_CYCLES(16UL*n)
#define DELAY_MS(n)     DELAY_CYCLES(16000UL*n);

// getLastError()
#define WDC_OK             0 // no error
#define WDC_TIMEOUT        1 // WDC controller timed out
#define WDC_DRIVENOTREADY  2 // /READY 1
#define WDC_WRITEFAULT     3 // /WFAULT 0
#define WDC_NOADDRMARK     4 // no address mark
#define WDC_NOSECTORID     5 // sector not found
#define WDC_DATAERROR      6 // CRC/ECC data error
#define WDC_BADBLOCK       7 // bad sector indicator
#define WDC_CORRECTED      8 // successful ECC correction

// DiskDriveParams.DataVerifyMode
#define MODE_CRC_16BIT     0
#define MODE_ECC_32BIT     1
#define MODE_ECC_56BIT     2

class WD42C22
{
public:
  static WD42C22* get()
  {
    static WD42C22 wdc;
    return &wdc;
  }
  
  // 15 bytes, needs to be POD
  struct DiskDriveParams
  {
    bool UseRLL;
    BYTE DataVerifyMode;
    WORD Cylinders;
    BYTE Heads;
    bool UseWritePrecomp;
    WORD WritePrecompStartCyl;
    bool UseReduceWriteCurrent;
    WORD RWCStartCyl;
    bool UseLandingZone;
    WORD LandingZone;
    bool SlowSeek;
  };
  
  // functions for buffer SRAM access  
  void sramBeginBufferAccess(bool, WORD);
  BYTE sramReadByteSequential();
  void sramWriteByteSequential(BYTE);
  void sramFinishBufferAccess();
  void sramClearBuffer(WORD count = 2048);
  
  DiskDriveParams* getParams() { return &m_params; }
  
  bool testBoard();
  void selectDrive(bool ds0 = true);
  bool isDriveReady();
  bool isWriteFault();
  bool isAtCylinder0();
  bool recalibrate();
  bool seekDrive(WORD, BYTE);  
  bool applyParams();
  void setWindowShift(bool, bool);
  
  WORD getPhysicalCylinder() { return m_physicalCylinder; }
  BYTE getPhysicalHead() { return m_physicalHead; }
  
  WORD getSectorSizeFromSDH(BYTE);
  BYTE getSDHFromSectorSize(WORD);
  
  BYTE getLastError() { return m_result; }
  BYTE getLastErrorMessage() { return m_errorMessage; } // Progmem index
  
  void scanID(WORD&, BYTE&, BYTE&);
  void readSector(BYTE, WORD, bool longMode = false, WORD* overrideCyl = NULL, BYTE* overrideHead = NULL);
  void verifyTrack(BYTE, WORD, BYTE startSector = 1, WORD* overrideCyl = NULL, BYTE* overrideHead = NULL);
  DWORD* fillSectorsTable(WORD&);
  bool prepareFormatInterleave(BYTE, BYTE, BYTE startSector = 1, BYTE* badBlocksTable = NULL);
  void formatTrack(BYTE, WORD, WORD* overrideCyl = NULL, BYTE* overrideHead = NULL);
  void writeSector(BYTE, WORD, WORD* overrideCyl = NULL, BYTE* overrideHead = NULL);
  void setBadSector(BYTE, WORD* overrideCyl = NULL, BYTE* overrideHead = NULL);
  
private:  
  WD42C22();
  
  // inlined functions for "microcontroller interface" access
  inline BYTE adRead(BYTE)                      __attribute__((always_inline));
  inline void adWrite(BYTE, BYTE)               __attribute__((always_inline));
  
  void resetController();
  void loadParameterBlock(BYTE, BYTE, bool useNonStandardSizes = false, WORD nonStandardSize = 0);
  void setParameter();
  void processResult();
  void computeCorrection();
  void doCorrection();
  
  bool m_seekForward;
  WORD m_physicalCylinder;
  BYTE m_physicalHead;
  BYTE m_result;
  BYTE m_errorMessage;
  
  DiskDriveParams m_params = {};
};