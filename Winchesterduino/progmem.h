// Winchesterduino (c) 2025 J. Bogin, http://boginjr.com
// Program memory data

#pragma once
#include "config.h"

// bleh
#define PROGMEM_STR inline static const unsigned char

class Progmem
{
public:
  
  // enum in sync with order of m_stringTable
  enum BYTE
  {
    // basic UI
    uiEmpty = 0,    
    uiDeleteLine,
    uiNewLine,
    uiNewLine2x,
    uiEchoKey,
    uiVT100ClearScreen,
    uiOK,
    uiFAIL,
    uiEnabled,
    uiDisabled,
    uiYes,
    uiNo,
    uiAbort,
    uiContinue,
    uiDecimalInput,
    uiDecimalInputEsc,
    uiBytes,
    uiOperationPending,
    uiChooseOption,
    uiChooseCylinder,
    uiChooseHead,
    uiChooseSector,
    uiChooseStartCyl,
    uiChooseEndCyl,
    uiChooseSecSize,
    uiEscGoBack,
    
    // errors
    uiFatalError,
    uiSystemHalted,
    uiFeSeek,
    uiFeTimeout,
    uiFeMemory,
    
    // non fatal errors    
    uiCHSInfo,
    uiCHInfo,
    uiHInfo,
    uiErrTimeout,
    uiErrNotReady,
    uiErrWriteFault,
    uiErrNoAddrMark,
    uiErrNoSectorID,
    uiErrDataCRC,
    uiErrDataECC,
    uiErrBadBlock,
    uiErrEccCorrected,
    
    // startup related   
    uiSplash,
    uiBuild,
    uiTestingWDC,
    uiWaitingReady,
    uiSeekingToCyl0,
    uiMinimalMode1,
    uiMinimalMode2,
    uiMinimalMode3,
    uiSetupParams,    
    uiSetupDataMode,
    uiSetupDataVerify,
    uiSetupCylinders,
    uiSetupHeads,
    uiSetupAskRWC,
    uiSetupCylRWC,
    uiSetupAskPrecomp,
    uiSetupCylPrecomp,
    uiSetupAskLZ,
    uiSetupCylLZ,
    uiSetupAskSeek,
    uiSetupAskSave,
    uiSetupAskRemove,    
    uiSetupSaved,
    uiSetupSavedLoad,
    uiShowFromCyl,
    uiShowSeekSlow,
    uiShowSeekFast,
    uiShowVerifyCRC,
    uiShowVerifyECC,
    uiShowVerifyECC56,
    uiShowDataMode,
    uiShowVerifyMode,
    uiShowCylinders,
    uiShowHeads,
    uiShowRWC,
    uiShowPrecomp,
    uiShowLZStatus,
    uiShowLZ,
    uiShowSeekMode,
    
    // minimal mode
    uiMinimalModeSeek1,
    uiMinimalModeSeek2,
    uiMinimalModeSeek3,
    
    // main menu
    optionAnalyze,
    optionHexdump,
    optionFormat,
    optionScan,
    optionReadImage,
    optionWriteImage,
    optionShowParams,
    optionDos,
    optionPark,
    
    // analyze command
    analyzePrintOrder,
    analyzeNoSectors,
    analyzeSectorInfo,
    analyzeSectorInfo2,
    analyzeSectorInfo3,
    analyzeSectorInfo4,
    analyzeSectorInfo5,
    analyzeCylHdNormal,
    analyzeConstSsize, 
    analyzeWarning,
    analyzeCylMismatch,
    analyzeHdMismatch,
    analyzeVarSsize,
    
    // hexdump command
    hexdumpLongMode,
    hexdumpDump,
    hexdumpChecksum,
    hexdumpPolynomial1,
    hexdumpPolynomial2,
    hexdumpPolynomial3,
    hexdumpChecksum2,
    hexdumpOk,
    
    // format command
    formatWarning,
    formatSpt,
    formatInterleave,
    formatVerify, 
    formatBadBlocks,
    formatProgress,
    formatComplete,
    
    // scan command
    scanWarning1,
    scanWarning2,
    scanWarning3,
    scanMarginal,
    scanProgress,
    
    // park command
    parkSuccess, 
    parkPowerdownSafe, 
    parkContinue,
    parkRecalibrating,
    
    // image file transfer
    imgXmodem1k,
    imgXmodemPrefix,
    imgXmodem1kPrefix,
    imgXmodemWaitSend, 
    imgXmodemWaitRecv,
    imgXmodemXferEnd,
    imgXmodemXferFail,
    imgXmodemErrPacket,
    imgXmodemErrHeader,
    imgXmodemErrParams,
    imgXmodemErrSecTyp,
    imgXmodemErrMFMRLL,
    imgXmodemErrCyls,
    imgXmodemErrHeads,
    imgXmodemErrVar1,
    imgXmodemErrVar2,
    imgWriteHeader,
    imgWriteComment,
    imgWriteDone,
    imgWriteEnterEsc,    
    imgBadBlocks,
    imgBadBlocksKnown,
    imgDataCorrected,
    imgDataErrors,
    imgDataErrorsConv,
    imgBadTracks,
    imgOverrideWrite1,
    imgOverrideWrite2,
    imgOverrideWrite3,
    imgBadBloxOption1,
    imgBadBloxOption2,
    imgDataErrorsOpt1,
    imgDataErrorsOpt2,
    imgDiskStats,
    imgImageStats,
    imgRunScan,
    imgRestoreParams,
    
    // DOS
    dosInvalidSsize,
    dosFsMountError,
    dosDiskError,
    dosFileNotFound,
    dosPathNotFound, 
    dosDirectoryFull,
    dosFileExists,
    dosFsError,    
    dosInvalidName,
    dosInvalidDirName,
    dosMaxPath,
    dosInvalidCommand,
    dosDirectory,
    dosDirectoryEmpty,
    dosBytesFormat, 
    dosBytesFree,
    dosTypeInto,
    dosMounted,
    dosCommands,
    dosCommandsList,
    dosForbiddenChars
  };
  
  // retrieve string from progmem, buffer valid until next call
  static const unsigned char* getString(unsigned char stringIndex)
  {   
    strncpy_P(m_strBuffer, pgm_read_ptr(&(m_stringTable[stringIndex])), MAX_PROGMEM_STRING_LEN);
    return (const unsigned char*)&m_strBuffer[0];
  }
  
// messages (definition order does not matter here). Length max MAX_PROGMEM_STRING_LEN
// variadics can be loaded only one at a time from PROGMEM,
// as only one buffer is used to transfer these between program space and our RAM
private:

// basic UI
  PROGMEM_STR m_uiEmpty[]            PROGMEM = "";
  PROGMEM_STR m_uiDeleteLine[]       PROGMEM = "\r                                                          \r";
  PROGMEM_STR m_uiNewLine[]          PROGMEM = "\r\n";
  PROGMEM_STR m_uiNewLine2x[]        PROGMEM = "\r\n\r\n";
  PROGMEM_STR m_uiEchoKey[]          PROGMEM = "%c\r\n";
  PROGMEM_STR m_uiVT100ClearScreen[] PROGMEM = "\e[H\e[2J\r                                                 \r";
  PROGMEM_STR m_uiOK[]               PROGMEM = "OK";
  PROGMEM_STR m_uiFAIL[]             PROGMEM = "FAIL";
  PROGMEM_STR m_uiEnabled[]          PROGMEM = "enabled";
  PROGMEM_STR m_uiDisabled[]         PROGMEM = "disabled";
  PROGMEM_STR m_uiYes[]              PROGMEM = "yes";
  PROGMEM_STR m_uiNo[]               PROGMEM = "no";
  PROGMEM_STR m_uiAbort[]            PROGMEM = "Press Esc now to abort.";
  PROGMEM_STR m_uiContinue[]         PROGMEM = "ENTER to continue...";
  PROGMEM_STR m_uiDecimalInput[]     PROGMEM = "0123456789\r\b";
  PROGMEM_STR m_uiDecimalInputEsc[]  PROGMEM = "0123456789\r\b\e";
  PROGMEM_STR m_uiBytes[]            PROGMEM = "bytes";
  PROGMEM_STR m_uiOperationPending[] PROGMEM = "Busy...";
  PROGMEM_STR m_uiChooseOption[]     PROGMEM = "Choose: ";
  PROGMEM_STR m_uiChooseCylinder[]   PROGMEM = "Cylinder (%u-%u): ";
  PROGMEM_STR m_uiChooseHead[]       PROGMEM = "Head (%u-%u): ";
  PROGMEM_STR m_uiChooseSector[]     PROGMEM = "Sector (%u-%u): ";
  PROGMEM_STR m_uiChooseStartCyl[]   PROGMEM = "Starting cylinder (%u-%u): ";
  PROGMEM_STR m_uiChooseEndCyl[]     PROGMEM = "Ending cylinder (%u-%u): ";
  PROGMEM_STR m_uiChooseSecSize[]    PROGMEM = "Sector size (1)28B (2)56B (5)12B 1(K)B: ";
  PROGMEM_STR m_uiEscGoBack[]        PROGMEM = "\r\nPress Esc to go back.\r\n";
  
// fatal errors
  PROGMEM_STR m_uiFatalError[]       PROGMEM = "\r\n\r\nFatal error:\r\n";
  PROGMEM_STR m_uiSystemHalted[]     PROGMEM = "\r\n\r\nSystem halted, reset required.";
  PROGMEM_STR m_uiFeSeek[]           PROGMEM = "Drive is not seeking properly!";
  PROGMEM_STR m_uiFeTimeout[]        PROGMEM = "Timeout from WDC: separator not present or missing WCLOCK.";
  PROGMEM_STR m_uiFeMemory[]         PROGMEM = "Memory allocation error!";
  
// non-fatal errors
  PROGMEM_STR m_uiCHSInfo[]          PROGMEM = "\r\nCylinder: %-4u Head: %-2u Sector: %-3u - ";
  PROGMEM_STR m_uiCHInfo[]           PROGMEM = "\r\nCylinder: %-4u Head: %-2u - ";
  PROGMEM_STR m_uiHInfo[]            PROGMEM = "\r\nHead %-2u - ";
  PROGMEM_STR m_uiErrTimeout[]       PROGMEM = "WDC not responding";
  PROGMEM_STR m_uiErrNotReady[]      PROGMEM = "Disk not ready";
  PROGMEM_STR m_uiErrWriteFault[]    PROGMEM = "Disk write fault signal";
  PROGMEM_STR m_uiErrNoAddrMark[]    PROGMEM = "No data address mark";
  PROGMEM_STR m_uiErrNoSectorID[]    PROGMEM = "Sector not found";
  PROGMEM_STR m_uiErrDataCRC[]       PROGMEM = "CRC data error";
  PROGMEM_STR m_uiErrDataECC[]       PROGMEM = "ECC data error";
  PROGMEM_STR m_uiErrBadBlock[]      PROGMEM = "Bad block";
  PROGMEM_STR m_uiErrEccCorrected[]  PROGMEM = "ECC corrected data";
  
// startup and setup related
  PROGMEM_STR m_uiSplash[]           PROGMEM = "Winchesterduino (c) 2025 J. Bogin\r\nBuild: ";
  PROGMEM_STR m_uiBuild[]            PROGMEM = "24th Jun 2025";
  PROGMEM_STR m_uiTestingWDC[]       PROGMEM = "Testing WD42C22 and its buffer RAM...";
  PROGMEM_STR m_uiWaitingReady[]     PROGMEM = "Waiting until drive becomes /READY...";
  PROGMEM_STR m_uiSeekingToCyl0[]    PROGMEM = "Determining position of cylinder 0...";
  PROGMEM_STR m_uiMinimalMode1[]     PROGMEM = "WDC disk controller not present, or not working properly.\r\n";
  PROGMEM_STR m_uiMinimalMode2[]     PROGMEM = "Running in minimal mode. Only basic seeking is supported.\r\n";
  PROGMEM_STR m_uiMinimalMode3[]     PROGMEM = "Use a logic analyzer on the 'raw TTL read data' connector.\r\n";
  PROGMEM_STR m_uiSetupParams[]      PROGMEM = "Set disk drive parameters:\r\n\r\n";
  PROGMEM_STR m_uiSetupDataMode[]    PROGMEM = "Data separator (M)FM / (R)LL: ";
  PROGMEM_STR m_uiSetupDataVerify[]  PROGMEM = "Data verify mode (1)6bit CRC / (3)2bit ECC / (5)6bit ECC: ";
  PROGMEM_STR m_uiSetupCylinders[]   PROGMEM = "Cylinders (1-2048): ";
  PROGMEM_STR m_uiSetupHeads[]       PROGMEM = "Heads (1-16): ";
  PROGMEM_STR m_uiSetupAskRWC[]      PROGMEM = "Drive requires reduced write current? Y/N: ";
  PROGMEM_STR m_uiSetupCylRWC[]      PROGMEM = "Reduced write current start cylinder (0: always): ";
  PROGMEM_STR m_uiSetupAskPrecomp[]  PROGMEM = "Drive requires write precompensation? Y/N: ";
  PROGMEM_STR m_uiSetupCylPrecomp[]  PROGMEM = "Write precompensation start cylinder (0: always): ";
  PROGMEM_STR m_uiSetupAskLZ[]       PROGMEM = "Is landing zone specified (no auto parking)? Y/N: ";
  PROGMEM_STR m_uiSetupCylLZ[]       PROGMEM = "Landing zone (parking cylinder): ";
  PROGMEM_STR m_uiSetupAskSeek[]     PROGMEM = "Drive seeking: (B)uffered / (S)T-506 compatible: ";
  PROGMEM_STR m_uiSetupAskSave[]     PROGMEM = "\r\nRemember these settings? Y/N: ";
  PROGMEM_STR m_uiSetupAskRemove[]   PROGMEM = "Remove saved settings? Y/N: ";
  PROGMEM_STR m_uiSetupSaved[]       PROGMEM = "Stored settings:\r\n";
  PROGMEM_STR m_uiSetupSavedLoad[]   PROGMEM = "\r\nLoading these settings.\r\n";
  PROGMEM_STR m_uiShowFromCyl[]      PROGMEM = ", from cylinder %u";
  PROGMEM_STR m_uiShowSeekSlow[]     PROGMEM = "ST-506 compatible\r\n";
  PROGMEM_STR m_uiShowSeekFast[]     PROGMEM = "buffered\r\n";
  PROGMEM_STR m_uiShowVerifyCRC[]    PROGMEM = "16-bit CRC";
  PROGMEM_STR m_uiShowVerifyECC[]    PROGMEM = "32-bit ECC";
  PROGMEM_STR m_uiShowVerifyECC56[]  PROGMEM = "56-bit ECC";
  PROGMEM_STR m_uiShowDataMode[]     PROGMEM = "\r\nData separator mode:   ";
  PROGMEM_STR m_uiShowVerifyMode[]   PROGMEM = "\r\nData verify mode:      ";
  PROGMEM_STR m_uiShowCylinders[]    PROGMEM = "\r\nCylinders:             %u";
  PROGMEM_STR m_uiShowHeads[]        PROGMEM = "\r\nHeads:                 %u";
  PROGMEM_STR m_uiShowRWC[]          PROGMEM = "\r\nReduced write current: ";
  PROGMEM_STR m_uiShowPrecomp[]      PROGMEM = "\r\nWrite precompensation: ";
  PROGMEM_STR m_uiShowLZStatus[]     PROGMEM = "\r\nAutopark on powerdown: ";
  PROGMEM_STR m_uiShowLZ[]           PROGMEM = "\r\nLanding zone cylinder: %u";
  PROGMEM_STR m_uiShowSeekMode[]     PROGMEM = "\r\nDrive seeking mode:    ";
  
// minimal mode
  PROGMEM_STR m_uiMinimalModeSeek1[] PROGMEM = "Seek to cylinder (0-2047): ";
  PROGMEM_STR m_uiMinimalModeSeek2[] PROGMEM = "Head (0-15): ";
  PROGMEM_STR m_uiMinimalModeSeek3[] PROGMEM = "Seek completed\r\n\r\n";
  
// main menu
  PROGMEM_STR m_optionAnalyze[]      PROGMEM = "(A)nalyze sector ID fields and interleave\r\n";
  PROGMEM_STR m_optionHexdump[]      PROGMEM = "(H)ex dump of one sector\r\n";
  PROGMEM_STR m_optionFormat[]       PROGMEM = "(F)ormat low-level\r\n";
  PROGMEM_STR m_optionScan[]         PROGMEM = "(M)ark data errors into bad blocks\r\n";
  PROGMEM_STR m_optionReadImage[]    PROGMEM = "(R)ead disk into WDI image\r\n";
  PROGMEM_STR m_optionWriteImage[]   PROGMEM = "(W)rite disk from WDI image\r\n";
  PROGMEM_STR m_optionShowParams[]   PROGMEM = "(S)how current drive settings\r\n";
  PROGMEM_STR m_optionDos[]          PROGMEM = "(I)nspect DOS primary partition\r\n";
  PROGMEM_STR m_optionPark[]         PROGMEM = "(P)ark drive heads\r\n"; 
  
// analyze command
  PROGMEM_STR m_analyzePrintOrder[]  PROGMEM = "Show logical sector numbers (interleave tables)? Y/N: ";
  PROGMEM_STR m_analyzeNoSectors[]   PROGMEM = "No valid sectors read";
  PROGMEM_STR m_analyzeSectorInfo[]  PROGMEM = "%u sectors, %u bytes each, ";
  PROGMEM_STR m_analyzeSectorInfo2[] PROGMEM = "%u sectors, VARIABLE SIZE!, ";
  PROGMEM_STR m_analyzeSectorInfo3[] PROGMEM = "unknown";
  PROGMEM_STR m_analyzeSectorInfo4[] PROGMEM = " interleave";
  PROGMEM_STR m_analyzeSectorInfo5[] PROGMEM = "\r\nOrder: ";
  PROGMEM_STR m_analyzeCylHdNormal[] PROGMEM = "ID field cylinder and head numbers match seek position.\r\n";
  PROGMEM_STR m_analyzeConstSsize[]  PROGMEM = "Sector sizes inside single tracks are consistent.\r\n";
  PROGMEM_STR m_analyzeWarning[]     PROGMEM = "Warning(s):\r\n";
  PROGMEM_STR m_analyzeCylMismatch[] PROGMEM = "*: ID field cylinder differs from the physical cylinder\r\n";
  PROGMEM_STR m_analyzeHdMismatch[]  PROGMEM = "@: ID field head differs from the physical head\r\n";
  PROGMEM_STR m_analyzeVarSsize[]    PROGMEM = "Variable sector size detected inside tracks!\r\n";
  
// hexdump command
  PROGMEM_STR m_hexdumpLongMode[]    PROGMEM = "Read with verify? Y/N: ";
  PROGMEM_STR m_hexdumpDump[]        PROGMEM = "Dump:\r\n";
  PROGMEM_STR m_hexdumpChecksum[]    PROGMEM = "%u-bit %s (datafield starts with A1 F8):\r\n";
  PROGMEM_STR m_hexdumpPolynomial1[] PROGMEM = "Initial: FFFF, polynomial: 1021\r\n"; 
  PROGMEM_STR m_hexdumpPolynomial2[] PROGMEM = "Initial: FFFFFFFF, polynomial: 140A0445\r\n"; 
  PROGMEM_STR m_hexdumpPolynomial3[] PROGMEM = "Initial: FFFFFFFFFFFFFF, polynomial: 140A0445000101\r\n"; 
  PROGMEM_STR m_hexdumpChecksum2[]   PROGMEM = "\r\nSector data not verified for integrity.\r\n";
  PROGMEM_STR m_hexdumpOk[]          PROGMEM = "Read OK";
  
// format command
  PROGMEM_STR m_formatWarning[]      PROGMEM = "\r\nDestroys data between specified cylinders.";
  PROGMEM_STR m_formatSpt[]          PROGMEM = "Sectors per track (%u-%u): ";
  PROGMEM_STR m_formatInterleave[]   PROGMEM = "Interleave (1: none): ";
  PROGMEM_STR m_formatVerify[]       PROGMEM = "Verify during format? Y/N: ";
  PROGMEM_STR m_formatBadBlocks[]    PROGMEM = "Mark *any* errors as bad blocks? Y/N: ";
  PROGMEM_STR m_formatProgress[]     PROGMEM = "\rFormatting cyl %u head %u... ";
  PROGMEM_STR m_formatComplete[]     PROGMEM = "\r\n\r\nFormat complete\r\n";
  
// scan command
  PROGMEM_STR m_scanWarning1[]       PROGMEM = "\r\nUse this to rescan bad sectors on a known good drive,\r\n";
  PROGMEM_STR m_scanWarning2[]       PROGMEM = "especially after writing a disk image to it.\r\n";
  PROGMEM_STR m_scanWarning3[]       PROGMEM = "Do NOT proceed if unsure about chosen drive settings.\r\n";
  PROGMEM_STR m_scanMarginal[]       PROGMEM = "Treat marginal (ECC correctable) sectors as bad? Y/N: ";
  PROGMEM_STR m_scanProgress[]       PROGMEM = "\rProcessing cylinder %u...";
  
// park command
  PROGMEM_STR m_parkSuccess[]        PROGMEM = "\rDrive heads sent to landing zone cylinder %u.\r\n";
  PROGMEM_STR m_parkPowerdownSafe[]  PROGMEM = "After powerdown, it is safe to relocate the drive.\r\n";
  PROGMEM_STR m_parkContinue[]       PROGMEM = "\r\nOr, press any key to resume working with the drive.\r\n";
  PROGMEM_STR m_parkRecalibrating[]  PROGMEM = "Recalibrating, please wait...";
    
// image file transfer
  PROGMEM_STR m_imgXmodem1k[]        PROGMEM = "Use XMODEM-1K? Y/N: ";
  PROGMEM_STR m_imgXmodemPrefix[]    PROGMEM = "XMODEM: ";
  PROGMEM_STR m_imgXmodem1kPrefix[]  PROGMEM = "XMODEM-1K: ";
  PROGMEM_STR m_imgXmodemWaitSend[]  PROGMEM = "OK to launch Send\r\nTimeout 4 minutes\r\n";
  PROGMEM_STR m_imgXmodemWaitRecv[]  PROGMEM = "OK to launch Receive\r\nTimeout 4 minutes\r\n";
  PROGMEM_STR m_imgXmodemXferEnd[]   PROGMEM = "\rEnd of transfer";
  PROGMEM_STR m_imgXmodemXferFail[]  PROGMEM = "\rTransfer aborted";
  PROGMEM_STR m_imgXmodemErrPacket[] PROGMEM = "Invalid XMODEM data packet";
  PROGMEM_STR m_imgXmodemErrHeader[] PROGMEM = "Invalid WDI file header";
  PROGMEM_STR m_imgXmodemErrParams[] PROGMEM = "Invalid drive parameters table in WDI file";
  PROGMEM_STR m_imgXmodemErrSecTyp[] PROGMEM = "Invalid sector data type in WDI file";
  PROGMEM_STR m_imgXmodemErrMFMRLL[] PROGMEM = "MFM<>RLL mismatch between image and current settings";
  PROGMEM_STR m_imgXmodemErrCyls[]   PROGMEM = "More physical cylinders in image than configured";
  PROGMEM_STR m_imgXmodemErrHeads[]  PROGMEM = "More physical heads in image than configured";  
  PROGMEM_STR m_imgXmodemErrVar1[]   PROGMEM = "WD42C22 cannot format varying sector sizes in 1 track!";
  PROGMEM_STR m_imgXmodemErrVar2[]   PROGMEM = "WD42C22 cannot format varying cyl/head numbers in 1 track!";
  PROGMEM_STR m_imgWriteHeader[]     PROGMEM = "WDI file created by Winchesterduino, (c) J. Bogin\r\n";
  PROGMEM_STR m_imgWriteComment[]    PROGMEM = "Add file comment (max %u characters per line)\r\n";
  PROGMEM_STR m_imgWriteDone[]       PROGMEM = "Type 2 empty newlines when done\r\n";
  PROGMEM_STR m_imgWriteEnterEsc[]   PROGMEM = "ENTER: continue, Esc: skip...";  
  PROGMEM_STR m_imgBadBlocks[]       PROGMEM = "%lu bad block(s),\r\n";
  PROGMEM_STR m_imgBadBlocksKnown[]  PROGMEM = "%lu pre-existing bad block(s),\r\n";
  PROGMEM_STR m_imgDataCorrected[]   PROGMEM = "%lu corrected ECC error(s),\r\n";
  PROGMEM_STR m_imgDataErrors[]      PROGMEM = "%lu uncorrectable CRC/ECC error(s).\r\n";
  PROGMEM_STR m_imgDataErrorsConv[]  PROGMEM = "%lu CRC/ECC error(s) converted to bad blocks.\r\n";
  PROGMEM_STR m_imgBadTracks[]       PROGMEM = "%lu unreadable track(s),\r\n";
  PROGMEM_STR m_imgOverrideWrite1[]  PROGMEM = "\r\nInspect the image with 'inspect.py', beforehand.";
  PROGMEM_STR m_imgOverrideWrite2[]  PROGMEM = "\r\nIf unsure, choose No on the following option.";
  PROGMEM_STR m_imgOverrideWrite3[]  PROGMEM = "\r\nLoad and override all drive settings from image? Y/N: ";
  PROGMEM_STR m_imgBadBloxOption1[]  PROGMEM = "\r\nSectors marked bad (bad blocks) in image:\r\n";
  PROGMEM_STR m_imgBadBloxOption2[]  PROGMEM = "Format as (E)mpty / Format as (B)ad on disk: ";
  PROGMEM_STR m_imgDataErrorsOpt1[]  PROGMEM = "\r\nSectors with CRC / ECC data errors in image:\r\n";
  PROGMEM_STR m_imgDataErrorsOpt2[]  PROGMEM = "Write as (G)ood data / Format as (E)mpty / Format as (B)ad: ";
  PROGMEM_STR m_imgDiskStats[]       PROGMEM = "Disk stats:\r\n";
  PROGMEM_STR m_imgImageStats[]      PROGMEM = "WDI image stats:\r\n";
  PROGMEM_STR m_imgRunScan[]         PROGMEM = "\r\nRun \"Mark data errors\" to re-scan defects on this disk.\r\n";
  PROGMEM_STR m_imgRestoreParams[]   PROGMEM = "(R)estore last disk settings or (K)eep those from image?: ";
  
// DOS  
  PROGMEM_STR m_dosInvalidSsize[]    PROGMEM = "Invalid sector size on track 0 (%u bytes)";
  PROGMEM_STR m_dosFsMountError[]    PROGMEM = "No primary DOS partition or not formatted";
  PROGMEM_STR m_dosDiskError[]       PROGMEM = "\rAborted due to disk error\r\n";  
  PROGMEM_STR m_dosFileNotFound[]    PROGMEM = "Not found in current path\r\n";
  PROGMEM_STR m_dosPathNotFound[]    PROGMEM = "Path not found\r\n";
  PROGMEM_STR m_dosDirectoryFull[]   PROGMEM = "Directory is not empty, or a file is read-only\r\n";
  PROGMEM_STR m_dosFileExists[]      PROGMEM = "Name already exists\r\n";
  PROGMEM_STR m_dosFsError[]         PROGMEM = "Filesystem error\r\n";  
  PROGMEM_STR m_dosInvalidName[]     PROGMEM = "Provide 1 valid file name";
  PROGMEM_STR m_dosInvalidDirName[]  PROGMEM = "Provide 1 valid directory name";
  PROGMEM_STR m_dosMaxPath[]         PROGMEM = "Path too deep";
  PROGMEM_STR m_dosInvalidCommand[]  PROGMEM = "Unrecognized command";
  PROGMEM_STR m_dosDirectory[]       PROGMEM = "          [DIR] ";
  PROGMEM_STR m_dosDirectoryEmpty[]  PROGMEM = "No files";
  PROGMEM_STR m_dosBytesFormat[]     PROGMEM = "%9lu ";
  PROGMEM_STR m_dosBytesFree[]       PROGMEM = "bytes free on disk.\r\n";  
  PROGMEM_STR m_dosTypeInto[]        PROGMEM = "Type two empty newlines to quit\r\n";
  PROGMEM_STR m_dosMounted[]         PROGMEM = "%u MB partition mounted.\r\n\r\n";
  PROGMEM_STR m_dosCommands[]        PROGMEM = "Supported commands:\r\n";
  PROGMEM_STR m_dosCommandsList[]    PROGMEM = "CD, DIR, MKDIR, RMDIR, DEL, TYPE, TYPEINTO, EXIT.\r\n\r\n";
  PROGMEM_STR m_dosForbiddenChars[]  PROGMEM = "*?\\/\":<>|";
   
// tables
private:

  PROGMEM_STR* const m_stringTable[] PROGMEM = {  
                                                  m_uiEmpty, m_uiDeleteLine, m_uiNewLine, m_uiNewLine2x, m_uiEchoKey,
                                                  m_uiVT100ClearScreen, m_uiOK, m_uiFAIL, m_uiEnabled, m_uiDisabled, m_uiYes,
                                                  m_uiNo, m_uiAbort, m_uiContinue, m_uiDecimalInput, m_uiDecimalInputEsc, 
                                                  m_uiBytes, m_uiOperationPending, m_uiChooseOption, m_uiChooseCylinder, m_uiChooseHead,
                                                  m_uiChooseSector, m_uiChooseStartCyl, m_uiChooseEndCyl, m_uiChooseSecSize,
                                                  m_uiEscGoBack,
                                                  
                                                  m_uiFatalError, m_uiSystemHalted, m_uiFeSeek, m_uiFeTimeout, m_uiFeMemory,
                                                  
                                                  m_uiCHSInfo, m_uiCHInfo, m_uiHInfo, m_uiErrTimeout, m_uiErrNotReady, m_uiErrWriteFault,
                                                  m_uiErrNoAddrMark, m_uiErrNoSectorID, m_uiErrDataCRC, m_uiErrDataECC, m_uiErrBadBlock,
                                                  m_uiErrEccCorrected,

                                                  m_uiSplash, m_uiBuild, m_uiTestingWDC, m_uiWaitingReady, m_uiSeekingToCyl0,
                                                  m_uiMinimalMode1, m_uiMinimalMode2, m_uiMinimalMode3, m_uiSetupParams, 
                                                  m_uiSetupDataMode, m_uiSetupDataVerify, m_uiSetupCylinders, m_uiSetupHeads,
                                                  m_uiSetupAskRWC, m_uiSetupCylRWC, m_uiSetupAskPrecomp, m_uiSetupCylPrecomp,
                                                  m_uiSetupAskLZ, m_uiSetupCylLZ, m_uiSetupAskSeek, m_uiSetupAskSave, m_uiSetupAskRemove, 
                                                  m_uiSetupSaved, m_uiSetupSavedLoad, m_uiShowFromCyl, m_uiShowSeekSlow, m_uiShowSeekFast,
                                                  m_uiShowVerifyCRC, m_uiShowVerifyECC, m_uiShowVerifyECC56, m_uiShowDataMode, 
                                                  m_uiShowVerifyMode, m_uiShowCylinders, m_uiShowHeads, m_uiShowRWC, m_uiShowPrecomp,
                                                  m_uiShowLZStatus, m_uiShowLZ, m_uiShowSeekMode,
                                                  
                                                  m_uiMinimalModeSeek1, m_uiMinimalModeSeek2, m_uiMinimalModeSeek3,
                                                  
                                                  m_optionAnalyze, m_optionHexdump, m_optionFormat, m_optionScan, m_optionReadImage,
                                                  m_optionWriteImage, m_optionShowParams, m_optionDos, m_optionPark,
                                                  
                                                  m_analyzePrintOrder, m_analyzeNoSectors, m_analyzeSectorInfo, m_analyzeSectorInfo2,
                                                  m_analyzeSectorInfo3, m_analyzeSectorInfo4, m_analyzeSectorInfo5, 
                                                  m_analyzeCylHdNormal, m_analyzeConstSsize, m_analyzeWarning,
                                                  m_analyzeCylMismatch, m_analyzeHdMismatch, m_analyzeVarSsize,
                                                  
                                                  m_hexdumpLongMode, m_hexdumpDump, m_hexdumpChecksum, m_hexdumpPolynomial1, 
                                                  m_hexdumpPolynomial2, m_hexdumpPolynomial3, m_hexdumpChecksum2, m_hexdumpOk,
                                                  
                                                  m_formatWarning, m_formatSpt, m_formatInterleave, m_formatVerify, m_formatBadBlocks,
                                                  m_formatProgress, m_formatComplete,
                                                  
                                                  m_scanWarning1, m_scanWarning2, m_scanWarning3, m_scanMarginal, m_scanProgress,
                                                  
                                                  m_parkSuccess, m_parkPowerdownSafe, m_parkContinue, m_parkRecalibrating,
                                                  
                                                  m_imgXmodem1k, m_imgXmodemPrefix, m_imgXmodem1kPrefix, m_imgXmodemWaitSend, 
                                                  m_imgXmodemWaitRecv, m_imgXmodemXferEnd, m_imgXmodemXferFail,                                                  
                                                  m_imgXmodemErrPacket, m_imgXmodemErrHeader, m_imgXmodemErrParams, m_imgXmodemErrSecTyp,
                                                  m_imgXmodemErrMFMRLL, m_imgXmodemErrCyls, m_imgXmodemErrHeads,                                                  
                                                  m_imgXmodemErrVar1, m_imgXmodemErrVar2, m_imgWriteHeader, m_imgWriteComment, 
                                                  m_imgWriteDone, m_imgWriteEnterEsc, m_imgBadBlocks, m_imgBadBlocksKnown, m_imgDataCorrected,
                                                  m_imgDataErrors, m_imgDataErrorsConv, m_imgBadTracks, m_imgOverrideWrite1, 
                                                  m_imgOverrideWrite2, m_imgOverrideWrite3, m_imgBadBloxOption1, m_imgBadBloxOption2,
                                                  m_imgDataErrorsOpt1, m_imgDataErrorsOpt2, m_imgDiskStats, m_imgImageStats, m_imgRunScan,
                                                  m_imgRestoreParams,
                                                  
                                                  m_dosInvalidSsize, m_dosFsMountError, m_dosDiskError, m_dosFileNotFound,
                                                  m_dosPathNotFound, m_dosDirectoryFull, m_dosFileExists, m_dosFsError, 
                                                  m_dosInvalidName, m_dosInvalidDirName, m_dosMaxPath, m_dosInvalidCommand, 
                                                  m_dosDirectory, m_dosDirectoryEmpty, m_dosBytesFormat, m_dosBytesFree, m_dosTypeInto,
                                                  m_dosMounted, m_dosCommands, m_dosCommandsList, m_dosForbiddenChars
                                               };

// provides for transfering messages between program space and our address space
// even though we're in class Progmem, this is in RAM, not in PROGMEM itself
private:

  inline static unsigned char m_strBuffer[MAX_PROGMEM_STRING_LEN + 1];
  
};