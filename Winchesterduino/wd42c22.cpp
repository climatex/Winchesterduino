// Winchesterduino (c) 2025 J. Bogin, http://boginjr.com
// WD42C22 Winchester Disk controller interface

#include "config.h"

// Pinouts Mega2560 to controller*, separator@, buffers$, disk drive%:
//
// Rightmost pinheader:
// PORTA7-A0 (D29-22): AD7-AD0*; input/output
// PORTC7-C0 (D30-37): /RESET*, RLL/MFM@, DIRECTION$, STEP$, HDSEL3$, HDSEL2$, HDSEL1$, HDSEL0$; outputs
// PORTL7-L0 (D42-49): /DACK*, /HCS*, /HWE*, /HRE*, ALE*, /MWE*, /MRE*, DS0$; outputs
// PORTG2-G0 (D39-41): n/c, n/c, SC@; output
//
// PWM 7-0 pinheader:
// PORTE4       (D02): /SC%;  input
// PORTE5       (D03): /MCINT*; input
// PORTG5       (D04): WINDOW@, output (initially input Hi-Z)
// PORTE3       (D05): WPCEN@,  output
// PORTH3       (D06): /READY%; input
// PORTH4       (D07): /TRK0%;  input

// interrupts - WDC "microcontroller interrupt" and drive "seek complete"
volatile bool mcintFired = false;
void MCINT()
{
  // on falling edge, set internal flag
  mcintFired = true;
}

volatile bool seekComplete = false;
void SC()
{
  // on change, invert drive /SC to SC output for the separator board, 
  // and also set our internal flag to signal we're done seeking
  if (PINE & 0x10)
  {
    PORTG &= 0xFE;
    seekComplete = false;
  }
  else
  {
    PORTG |= 1;
    seekComplete = true;
  }
}

// singleton, initial port settings during setup()
WD42C22::WD42C22()
{  
  cli();
 
  m_seekForward = false;
  m_physicalCylinder = 0;
  m_physicalHead = 0;
  m_result = WDC_OK;
  m_errorMessage = 0;
  
  // AD0-7 default to inputs, Hi-Z  
  PORTA = 0;
  DDRA = 0;
  
  // WDC after powerup asserts /RESET, so treat it as input pullup for now
  // outputs: RLL/MFM, DIRECTION, STEP and HDSEL3..0 all low
  // WD10C20B: RLL/MFM floating
  PORTC = 0x80;
#if defined(WD10C20B) && (WD10C20B == 1)
  DDRC = 0x3F;
#else
  DDRC = 0x7F;
#endif
  
  // outputs: /DACK, /HCS, /HWE, /HRE all high, ALE low, /MWE, /MRE all high, DS0 low
  PORTL = 0xF6;
  DDRL = 0xFF;
  
  // input: WINDOW Hi-Z, output: SC low (seek complete output to separator board)
  PORTG = 0;
  DDRG = 1;
  
  // input: drive /SC and controller /MCINT both input pullup (drive /SC also has external pullup)
  // output: WPCEN output low 
  PORTE = 0x30;
  DDRE = 8;
  
  // input: drive /READY and /TRK0 both input pullup (both also have external pullups)
  PORTH = 0x18;
  DDRH = 0;
  
  // during WDC powerup, /RESET assert can take up to 51ms
  DELAY_MS(51);
  resetController(); // and set initial options
  
  // /SC from drive and /MCINT from WDC
  attachInterrupt(digitalPinToInterrupt(2), SC, CHANGE);  
  attachInterrupt(digitalPinToInterrupt(3), MCINT, FALLING);
  
  sei();
}

void WD42C22::resetController()
{
  // write to Drive Interface Control and set:
  // bit 7 RDC - Reset Drive Controller,
  // bit 2 DRO - Disable Reset Output (disable reset out thru "host" on WDC side)
  adWrite(0x3B, 0x84);
  
  // write Interface Control Register and set all "reset bits"
  adWrite(0x38, 0x7F);
  
  // master reset for at least 24 write clock periods
  PORTC &= 0x7F; // bring /RESET low
  DDRC |= 0x80;  // make sure we're output
  DELAY_US(10);
  PORTC |= 0x80; // /RESET high
  
  // now set proper settings, there are a lot of bits not affected by any of these "resets"...     
  // write to Buffer Control Register, set AT/XT=1 "AT interface mode"
  adWrite(0x37, 0x80);
  
  // write to AT Control Register, ADQ7 and ADRQ (Auto DRQ) all disabled
  adWrite(0x2A, 0);
  
  // write to Auxiliary Buffer Control Register and set everything default (PIO mode etc)
  // except: clear bit 0 SCKS - switch drive controller from BCLK to WCLK from data separator
  // "The microcontroller must switch the clock source tothe WCLK input prior to issuing any commands to the drive controller."
  adWrite(0x2F, 0);
  
  // write to Digital Input Register, set DDRQ=1 (disable BDRQ interrupts)
  // driveselect handled by ourselves
  adWrite(0x3F, 0x40);  
  
  mcintFired = false; // reset flags
  seekComplete = false;
}

// *** read and write to individual registers of the WDC WD42C22 ***

// WDC distinguishes between 2 interfaces: "host" and "local micro(controller)"
// "host" was meant the actual PC ISA bus, connected through an address decoder, to WDC lines HD0-15 (8-bit or 16-bit)
// "micro" referred to a helper MCU (WD1017, etc.), soldered on the MFM controller card, connected to separate WDC lines AD0-7 (always 8-bit)
// WDC supports all of its commands and registers exclusively through the "micro" interface, and only exposes a few to the host PC bus!

// since there's no interface distinction here, all the WDC commands, registers, and data is supplied through the AD lines (adRead, adWrite)
// and the "host" interface is not used

// read and write registers of the "local microcontroller" interface AD0-7
BYTE WD42C22::adRead(BYTE reg)
{
  PORTL ^= 8;      // toggle ALE high
  PORTA = reg;
  DDRA = 0xFF;     // AD0-7 output address
  PORTL ^= 8;      // toggle ALE low
  PORTA = 0;   
  DDRA = 0;        // AD0-7 input Hi-Z
  PORTL ^= 2;      // toggle /MRE low
  DELAY_CYCLES(2); // data valid from /MRE can take up to 100ns
  const BYTE value = PINA;
  PORTL ^= 2;      // toggle /MRE high
  
  return value;
}

void WD42C22::adWrite(BYTE reg, BYTE value)
{
  PORTL ^= 8;      // toggle ALE high
  PORTA = reg;
  DDRA = 0xFF;     // AD0-7 output address
  PORTL ^= 8;      // toggle ALE low
  PORTL ^= 4;      // toggle /MWE low
  PORTA = value;   // AD0-7 output value
  DELAY_CYCLES(1); // data setup to /MWE high min. 50ns
  PORTL ^= 4;      // toggle /MWE high
  PORTA = 0;   
  DDRA = 0;        // AD0-7 input Hi-Z
}

void WD42C22::sramBeginBufferAccess(bool write /* false: read */, WORD startingOffset)
{
  BYTE bcr = adRead(0x37);                    // access Buffer Control and Interface Control registers
  BYTE icr = adRead(0x3B); 
  
  icr &= 0xF7;
  adWrite(0x3B, icr);                         // make sure MAC = 0 before changing DRWB
  if (write)
  {
    bcr &= 0xFB;                              // DRWB = 0 for writing into RAM
  }
  else
  {
    bcr |= 4;                                 // DRWB = 1, read
  }  
  adWrite(0x37, bcr);
  
  icr |= 8;
  adWrite(0x3B, icr);                         // MAC = 1
  adWrite(0x34, (BYTE)startingOffset);        // starting address 0 LSB
  adWrite(0x35, (BYTE)(startingOffset >> 8)); // MSB
  
  bcr |= 1;
  adWrite(0x37, bcr);                         // ADBP = 1
}

BYTE WD42C22::sramReadByteSequential()
{
  // read one byte from the current offset and advance it; buffer needs to be prepared
  return adRead(0x36);
}

void WD42C22::sramWriteByteSequential(BYTE value)
{
  adWrite(0x36, value);
}

void WD42C22::sramFinishBufferAccess()
{
  BYTE bcr = adRead(0x37);
  BYTE icr = adRead(0x3B);
  
  icr &= 0xF7;
  adWrite(0x3B, icr);                       // set MAC = 0 for proper controller operation
  bcr &= 0xFE;
  adWrite(0x37, bcr);                       // ADBP = 0
}

void WD42C22::sramClearBuffer(WORD count)
{
  sramBeginBufferAccess(true, 0);
  for (WORD index = 0; index < count; index++)
  {
    sramWriteByteSequential(0);
  }
  sramFinishBufferAccess();
}

bool WD42C22::testBoard()
{
  // a simple test of both the WDC chip and its associated 2K buffer SRAM (6116)
  const WORD sizeToTest = 2048;
  BYTE* testArray = new BYTE[sizeToTest];
  if (!testArray)
  {
    ui->fatalError(Progmem::uiFeMemory);
  }
  
  // write 2K of random values to our test array and to the buffer starting at offset 0
  sramBeginBufferAccess(true, 0);
  for (WORD index = 0; index < sizeToTest; index++)
  {
    testArray[index] = (BYTE)random(0, 256);
    sramWriteByteSequential(testArray[index]);
  }
  
  // now setup WDC to read from its buffer
  sramBeginBufferAccess(false, 0);    
  for (WORD index = 0; index < sizeToTest; index++)
  {
    // mismatch?
    if (testArray[index] != sramReadByteSequential())
    {
      delete[] testArray;
      return false; // WDC not present, not working properly or SRAM error
    }
  }
  
  sramClearBuffer(); // and also finish access to SRAM - important
  delete[] testArray; 
  return true;
}

void WD42C22::selectDrive(bool ds0 /* = true */)
{
  // false: unselects
  // drive must be selected before any operation on it
  if (ds0)
  {
    PORTL |= 1;
  }
  else
  {
    PORTL &= 0xFE;
  }
}

bool WD42C22::isDriveReady()
{
  // query /READY of the disk drive directly, don't read WDC status bits
  // as we want this function to work without the controller
   
  // needs to be consistently low for quite a while (about 120ms)
  // as it can fire momentarily during disk powerup 
  DWORD wait = 200000UL;
  while (--wait)
  {
    if ((PINH & 8) != 0)
    {
      return false; // not ready, quit instantly
    }    
  }
  
  return !isWriteFault(); // to be considered ready, /WFAULT must be also high
}

bool WD42C22::isAtCylinder0() // as above
{
  return (PINH & 0x10) == 0;
}

bool WD42C22::isWriteFault()
{
  return adRead(0x3B) & 0x20;
}

// find cylinder 0 during board initialization
// 1 slow seek step + wait for the head settle each singlestep
bool WD42C22::recalibrate()
{ 
  // make sure the drive is selected
  selectDrive();
  
  // select head 0
  PORTC &= 0xF0;
  m_physicalHead = 0;
  
  // seek not required?
  if (isAtCylinder0())
  {
    m_physicalCylinder = 0; 
    return true;
  }
   
  // a maximum of 2048 cylinders can be configured
  WORD attempts = 2048;
  while (!isAtCylinder0()) // inspect /TRK0 each head settle
  {
    if (!attempts)
    {
      ui->fatalError(Progmem::uiFeSeek);
      return false;
    }
    
    // direction change
    if (m_seekForward)
    {
      PORTC &= 0xDF; // DIRECTION low to seek towards 0 (inverted on drive, /DIRECTION)
      DELAY_CYCLES(1);
      m_seekForward = false;
    }    
    
    attempts--;
    PORTC ^= 0x10;
    DELAY_US(SEEK_PULSE_US);
    PORTC ^= 0x10;
    
    // wait for the head to settle each step (ST506 18ms typical)
    DELAY_MS(20);
  }

  m_physicalCylinder = 0;  
  return true;
}

// seek to given cylinder and head, set reduced write current or write precompensation line
bool WD42C22::seekDrive(WORD toCylinder, BYTE toHead)
{ 
  // make sure the drive is selected
  selectDrive();
  
  // set head
  if ((m_physicalHead != toHead) && (toHead < m_params.Heads))
  {
    PORTC = (PORTC & 0xF0) | toHead;
    m_physicalHead = toHead;
  }
  
  // set cylinder
  if ((m_physicalCylinder != toCylinder) && (toCylinder < m_params.Cylinders))
  {
    const bool forwards = toCylinder > m_physicalCylinder;
    WORD count = forwards ? (toCylinder - m_physicalCylinder) : (m_physicalCylinder - toCylinder);
    
    // direction change
    if (m_seekForward != forwards)
    {
      if (forwards)
      {
        PORTC |= 0x20;
      }
      else
      {
        PORTC &= 0xDF;
      }
      
      DELAY_CYCLES(1);
      m_seekForward = forwards;
    }
    
    // ST506 or buffered seek
    const bool slowSeek = m_params.SlowSeek;
    seekComplete = false;
    while (count)
    {
      PORTC ^= 0x10;
      DELAY_US(SEEK_PULSE_US);
      PORTC ^= 0x10;
      
      if (slowSeek)
      {
        DELAY_MS(SLOWSEEK_SRT_MS);  
      }
      else
      {
        DELAY_US(FASTSEEK_SRT_US);
      }
      
      count--;
    }
    
    // wait until heads are settled after all seek pulses are done
    DWORD wait = 800000UL;
    while (!seekComplete)
    {
      if (!--wait)
      {
        ui->fatalError(Progmem::uiFeSeek);
        return false;
      }    
    }
    
    m_physicalCylinder = toCylinder;
  }
  
  // the MSB HDSEL is on some very old drives used as reduced write current signal
  if (m_params.UseReduceWriteCurrent && (m_params.Heads <= 8))
  {
    if (m_physicalCylinder >= m_params.RWCStartCyl)
    {
      PORTC |= 8;    // HDSEL3 high (low /REDWC on drive)
    }
    else
    {
      PORTC &= 0xF7; // /REDWC on drive high
    }
  }
  
  // handle write precompensation line to data separator
  if (m_params.UseWritePrecomp)
  {
    if (m_physicalCylinder >= m_params.WritePrecompStartCyl)
    {
      PORTE |= 8;    // WPCEN high
    }
    else
    {
      PORTE &= 0xF7; // WPCEN low
    }
  }
  
  return true;
}

bool WD42C22::applyParams()
{
  // if drive parameters changed, some need to be updated to the WDC
  loadParameterBlock(m_params.UseRLL ? 0x33 : 0x4E, 0); // gaps contain byte 0x4E if MFM, 0x33 if RLL; paddings contain zeros
  if (getLastError() == WDC_TIMEOUT)
  {
    return false;
  }
  
  setParameter();
  if (getLastError() == WDC_TIMEOUT)
  {
    return false;
  } 

  // signal RLL/MFM line to separator
  if (m_params.UseRLL)
  {
    PORTC |= 0x40;
  }
  else
  {
    PORTC &= 0xBF;
  }
  
  return true;
}

void WD42C22::setWindowShift(bool enable, bool late)
{
  // set WINDOW (WSHIFT line of the data separator board)
  // disabled: Hi-Z
  // enabled: output high - shift data detection window late,
  //          output low  - shift window early
  
  if (!enable)
  {
    DDRG &= 0xDF;
    PORTG &= 0xDF;
  }
  else
  {
    DDRG |= 0x20;
    if (late)
    {
      PORTG |= 0x20;
    }
    else
    {
      PORTG &= 0xDF;
    }    
  }
}

void WD42C22::setParameter()
{
  // WD "set parameter" command
  // defaults here: no ESDI/NRZ, no relocation ID searches, 3bit head select SDH
  BYTE command = 0;
  if (m_params.Heads > 8)
  {
    command |= 2; // 4bit head select SDH
  }
  if (m_params.DataVerifyMode == MODE_ECC_56BIT)
  {
    command |= 4; // E=1  
  }
  
  m_result = WDC_OK;
  DWORD wait = 5000000UL;
  mcintFired = false;
  adWrite(0x27, command);
  
  while (!mcintFired)
  {
    if (!--wait)
    {
      m_result = WDC_TIMEOUT;
      break;
    }
  }
  
  processResult();
}

void WD42C22::loadParameterBlock(BYTE dataFillGaps, BYTE dataFillPads, bool useNonStandardSizes, WORD nonStandardSize)
{
  // WD "load parameter block" command
  // defaults here: soft sector MFM mode, standard (non-custom) sector sizes (bit 1, U=0)
  BYTE command = 0x8A;
  if (m_params.UseRLL)
  {
    command &= 0xFD; // K=0, RLL
  }
  if (useNonStandardSizes) // non-standard sector sizes / writeID offset specified
  {
    command |= 1; // U=1
  }
  
  // other registers that need to be set before issuing command
  adWrite(0x22, dataFillGaps); // sector count register contains data byte that will be filled into GAPs during format
  adWrite(0x23, dataFillPads); // sector number register contains data byte that will be filled into ID and DATA pads during format and write
  
  // cylinder number will contain non-standard sector size or writeID offset LSB and MSB
  if (useNonStandardSizes)
  {
    adWrite(0x24, (BYTE)nonStandardSize);
    adWrite(0x25, (BYTE)(nonStandardSize >> 8));
  }
    
  m_result = WDC_OK;
  DWORD wait = 5000000UL;
  mcintFired = false;
  adWrite(0x27, command);
  
  while (!mcintFired)
  {
    if (!--wait)
    {
      m_result = WDC_TIMEOUT;
      break;
    }
  }
  
  processResult();
}

void WD42C22::processResult()
{
  if (m_result == WDC_TIMEOUT) // already set within the particular function
  {
#if defined(WDC_TIMEOUT_HALT) && (WDC_TIMEOUT_HALT == 1)
    ui->fatalError(Progmem::uiFeTimeout);
#endif  

    m_errorMessage = Progmem::uiErrTimeout; 
    return;
  }
  
  const BYTE statusReg = adRead(0x27);
  const BYTE errorReg = adRead(0x21);  
  if ((statusReg & 1) == 0)    // ERR = 0
  {
    m_errorMessage = Progmem::uiEmpty;
    return;
  }
  
  if ((statusReg & 0x40) == 0) // RDY = 0
  {
    m_result = WDC_DRIVENOTREADY;
    m_errorMessage = Progmem::uiErrNotReady;
  }
  else if (statusReg & 0x20)   // WF = 1
  {
    m_result = WDC_WRITEFAULT;
    m_errorMessage = Progmem::uiErrWriteFault;
  }
  else if (errorReg & 0x80)    // BB = 1
  {
    m_result = WDC_BADBLOCK;
    m_errorMessage = Progmem::uiErrBadBlock;
  }
  else if (errorReg & 0x40)    // CRC/ECC = 1
  {
    m_result = WDC_DATAERROR;
    if (m_params.DataVerifyMode == MODE_CRC_16BIT)
    {
      m_errorMessage = Progmem::uiErrDataCRC;
    }
    else
    {
      m_errorMessage = Progmem::uiErrDataECC;
    }    
  }
  else if (errorReg & 0x10)    // IDNF = 1
  {
    m_result = WDC_NOSECTORID;
    m_errorMessage = Progmem::uiErrNoSectorID;
  }
  else if (errorReg & 1)       // DMNF = 1
  {
    m_result = WDC_NOADDRMARK;
    m_errorMessage = Progmem::uiErrNoAddrMark;
  }
}

void WD42C22::scanID(WORD& cylinderNo, BYTE& sectorNo, BYTE& sdh)
{ 
  // leave only sector size and head number bits (3 or 4, as set in setParameter) 
  const BYTE cancelSdh = (m_params.Heads > 8) ? 0x6F : 0x67;
  
  m_result = WDC_OK;
  DWORD wait = 5000000UL;
  mcintFired = false;
  adWrite(0x27, 0x40); // WD "scan ID" of whatever's flying thru the drive head at current cylinder
  
  while (!mcintFired)
  {
    if (!--wait)
    {
      m_result = WDC_TIMEOUT;
      break;
    }
  }
  
  processResult();
  if (!getLastError())
  {
    cylinderNo = (((WORD)adRead(0x25)) << 8) | adRead(0x24);
    sectorNo = adRead(0x23);
    sdh = adRead(0x26) & cancelSdh;
  }
  else
  {
    cylinderNo = 0;
    sectorNo = 0;
    sdh = 0;
  }
}

DWORD* WD42C22::fillSectorsTable(WORD& tableCount)
{
  // similar to above, fill a table of sector IDs
  // always returns the table on success or error - no checking, needs to be quick
  // deallocation handled by caller
  const BYTE cancelSdh = (m_params.Heads > 8) ? 0x6F : 0x67;
  
  tableCount = 100; // should suffice
  DWORD* table = new DWORD[tableCount];
  if (!table)
  {
    tableCount = 0;
    ui->fatalError(Progmem::uiFeMemory);
    return NULL;
  }
  memset(table, 0xFF, tableCount*sizeof(DWORD)); // each 0xFFFFFFFF value means unfilled due to error
  
  WORD tableIndex = 0;  
  while (tableIndex < tableCount)
  {
    DWORD wait = 5000000UL;
    mcintFired = false;
    adWrite(0x27, 0x40);
    
    while (!mcintFired)
    {
      if (!--wait)
      {
        return table;
      }
    }
    
    // AC (aborted command) == 0
    if ((adRead(0x21) & 4) == 0)
    {
      // each entry lo-WORD: cylinder number, hi-WORD: (MSB: SDH, LSB: sector number)
      table[tableIndex++] = (((((DWORD)adRead(0x26) & cancelSdh) << 24) | (DWORD)adRead(0x23) << 16)) | ((((WORD)adRead(0x25)) << 8) | adRead(0x24));
    }
    else
    {
      return table;
    }
  }
  
  return table;
}

void WD42C22::readSector(BYTE sectorNo, WORD sectorSizeBytes, bool longMode, WORD* overrideCyl, BYTE* overrideHead)
{
  // read sector of the current track and head into the buffer
  // sectorSizeBytes: 128, 256, 512, 1024 currently
  // longMode: do not check ECC/CRC; instead, append the 4 or 7 checksum bytes into the buffer
  // overrideCyl, overrideHead: logical sector information differs from the physical cylinder and head
   
  BYTE bcr = adRead(0x37);
  BYTE icr = adRead(0x3B);
  
  icr &= 0xF7;
  adWrite(0x3B, icr);      // make sure MAC = 0 before changing DRWB    
  bcr &= 0xFB;             // DRWB = 0
  adWrite(0x37, bcr);
  adWrite(0x34, 0);        // starting address of data into the buffer
  adWrite(0x35, 0);
  adWrite(0x3F, 0x40);     // ECCM = 0, DDRQ = 1
  icr |= 8;
  adWrite(0x3B, icr);      // MAC = 1  
  bcr |= 1;
  adWrite(0x37, bcr);      // ADBP = 1  
  icr &= 0xF7;
  adWrite(0x3B, icr);      // MAC = 0
  
  WORD currentCyl = m_physicalCylinder;
  BYTE currentHead = m_physicalHead;
  if (overrideCyl)
  {
    currentCyl = *overrideCyl;
  }
  if (overrideHead)
  {
    currentHead = *overrideHead;
  }
  
  // prepare task file registers  
  adWrite(0x23, sectorNo);                // sector number
  adWrite(0x24, (BYTE)currentCyl);        // LSB
  adWrite(0x25, (BYTE)(currentCyl >> 8)); // MSB
  
  // prepare SDH register
  BYTE sdh = getSDHFromSectorSize(sectorSizeBytes);

  // ECC = 1 into SDH  
  if (m_params.DataVerifyMode != MODE_CRC_16BIT)
  {
    sdh |= 0x80;
  }
  sdh |= currentHead; // low 3 or 4 bits
  adWrite(0x26, sdh);
  
  BYTE command = 0x20; // read
  if (longMode)
  {
    command |= 2;      // L=1
  }

  m_result = WDC_OK;
  DWORD wait = 5000000UL;
  mcintFired = false;
  adWrite(0x27, command);
  
  while (!mcintFired)
  {
    if (!--wait)
    {
      m_result = WDC_TIMEOUT;
      break;
    }
  }
  
  processResult();
  
  // try to correct ECC error
  if ((getLastError() == WDC_DATAERROR) && (m_params.DataVerifyMode != MODE_CRC_16BIT))
  {
    computeCorrection();
    
    // correctable?
    if (getLastError() == WDC_CORRECTED)
    {
      doCorrection();
    }
  }
}

void WD42C22::verifyTrack(BYTE sectorsPerTrack, WORD sectorSizeBytes, WORD* overrideCyl, BYTE* overrideHead)
{
  // as above, but reads up to sectorsPerTrack of constant sectorSizeBytes
  // the SRAM buffer is too small for whole track reads, and its contents are trashed
  // used for quick verify during mainmenu format:
  // if this fails, fall back to individual readSector to determine offending sectors
  
  BYTE bcr = adRead(0x37);
  BYTE icr = adRead(0x3B);
  
  icr &= 0xF7;
  adWrite(0x3B, icr);      // make sure MAC = 0 before changing DRWB    
  bcr &= 0xFB;             // DRWB = 0
  adWrite(0x37, bcr);
  adWrite(0x34, 0);        // starting address of data into the buffer
  adWrite(0x35, 0);
  adWrite(0x3F, 0x40);     // ECCM = 0, DDRQ = 1
  icr |= 8;
  adWrite(0x3B, icr);      // MAC = 1  
  bcr |= 1;
  adWrite(0x37, bcr);      // ADBP = 1  
  icr &= 0xF7;
  adWrite(0x3B, icr);      // MAC = 0
  
  WORD currentCyl = m_physicalCylinder;
  BYTE currentHead = m_physicalHead;
  if (overrideCyl)
  {
    currentCyl = *overrideCyl;
  }
  if (overrideHead)
  {
    currentHead = *overrideHead;
  }
  
  // prepare task file registers  
  adWrite(0x22, sectorsPerTrack);         // sector count
  adWrite(0x23, 1);                       // starting at sector 1
  adWrite(0x24, (BYTE)currentCyl);        // LSB
  adWrite(0x25, (BYTE)(currentCyl >> 8)); // MSB
  
  // prepare SDH register
  BYTE sdh = getSDHFromSectorSize(sectorSizeBytes);

  // ECC = 1 into SDH  
  if (m_params.DataVerifyMode != MODE_CRC_16BIT)
  {
    sdh |= 0x80;
  }
  sdh |= currentHead; // low 3 or 4 bits
  adWrite(0x26, sdh);
  
  m_result = WDC_OK;
  DWORD wait = 5000000UL;
  mcintFired = false;
  adWrite(0x27, 0x24); // read multisector
  
  while (!mcintFired)
  {
    if (!--wait)
    {
      m_result = WDC_TIMEOUT;
      break;
    }
  }
  
  processResult();  
}

void WD42C22::computeCorrection()
{
  // only valid for ECC modes
  
  BYTE bcr = adRead(0x37);
  BYTE icr = adRead(0x3B);
  
  icr &= 0xF7;
  adWrite(0x3B, icr);      // make sure MAC = 0 before changing DRWB    
  bcr &= 0xFB;             // DRWB = 0
  adWrite(0x37, bcr);
  adWrite(0x34, 0xF0);     // starting address of error correction bytes: offset 2032 
  adWrite(0x35, 7);        // (max 1K sectors supported atm)
  adWrite(0x3F, 0x40);     // ECCM = 0, DDRQ = 1
  icr |= 8;
  adWrite(0x3B, icr);      // MAC = 1  
  bcr |= 1;
  adWrite(0x37, bcr);      // ADBP = 1  
  icr &= 0xF7;
  adWrite(0x3B, icr);      // MAC = 0
  
  m_result = WDC_OK;
  DWORD wait = 5000000UL;
  mcintFired = false;
  adWrite(0x27, 8);        // compute correction
  
  while (!mcintFired)
  {
    if (!--wait)
    {
      m_result = WDC_TIMEOUT;
      break;
    }
  }
  
  processResult();         // if still WDC_DATAERROR, it is an uncorrectable error and the computed data are not helpful
  if (m_result != WDC_DATAERROR)
  {
    m_result = WDC_CORRECTED;
    m_errorMessage = Progmem::uiErrEccCorrected;
  }
}

void WD42C22::doCorrection()
{
  // max 7 syndrome bytes (unused here) + 2 byte offset + max 7 error pattern bytes
  BYTE data[16] = {0};
  
  // retrieve error correction bytes from SRAM buffer offset 2032 (last 16 bytes)
  sramBeginBufferAccess(false, 2032);
  for (BYTE index = 0; index < 16; index++)
  {
    data[index] = sramReadByteSequential();
  }
  
  const WORD errorLocation = ((WORD)(data[7]) << 8) | data[8]; // after syndrome bytes
  const BYTE eccSize = (m_params.DataVerifyMode == MODE_ECC_56BIT) ? 7 : 4;
  
  // 4 byte ECC: default correction span of 5 bits, XOR first two error pattern bytes
  BYTE spanningCorrection = data[9] ^ data[10];  
  if (eccSize == 7) // 7-byte ECC: XOR also the third byte
  {
    spanningCorrection ^= data[11];
  }
  
  // read faulty disk data, correct it with the error pattern, and place corrected data into the (unused) syndrome bytes
  sramBeginBufferAccess(false, errorLocation);
  for (BYTE index = 0; index < eccSize; index++)
  {
    data[index] = sramReadByteSequential() ^ data[index+9];
  }  
  data[0] ^= spanningCorrection;
  
  // we can only read or only write the SRAM buffer, and that in a single direction, so now write the corrected data back
  sramBeginBufferAccess(true, errorLocation);
  for (BYTE index = 0; index < eccSize; index++)
  {
    sramWriteByteSequential(data[index]);
  }
  
  // done
  sramFinishBufferAccess();
}

WORD WD42C22::getSectorSizeFromSDH(BYTE sdh)
{ 
  sdh &= 0x60;
  switch(sdh)
  {
    case 0x60:
      return 128;
    case 0x40:
      return 1024;
    case 0x20:
      return 512;
    default:
      return 256; // or U=1
  }
}

BYTE WD42C22::getSDHFromSectorSize(WORD sectorSizeBytes)
{
  BYTE sdh = 0;
  
  switch(sectorSizeBytes)
  {
  case 128:
    sdh = 0x60;
    break;
  case 1024:
    sdh = 0x40;
    break;
  case 512:
    sdh = 0x20;
  default:
    break; // 256 or U=1
  }
  
  return sdh;
}

bool WD42C22::prepareFormatInterleave(BYTE sectorsPerTrack, BYTE interleave, BYTE* badBlocksTable)
{
  // writes a special interleave table for the WDC into its buffer
  // 2 bytes per each sector, structure:
  // 1st physical sector: X Y, then 2nd physical sector: X Y, then 3rd... etc
  // X is 0 for normal sectors or 0x80 for bad sector marks, Y is the logical sector number or 0xFF if a 0x80 bad mark was used
  
  // sectorsPerTrack: number of physical sectors per track
  // badBlocksTable: if not null, points to an array of bytes, sectorsPerTrack size
  // array index is physical sector index, not its interleaved value
  // e.g. badBlocksTable[0] nonzero, [1] zero: first sector on track is marked bad, second is good
  
  BYTE* interleaveTable = NULL; // fallback to sequential on invalid values
  if ((interleave > 1) && (interleave < sectorsPerTrack))
  {
    interleaveTable = new BYTE[sectorsPerTrack + 1];
    if (!interleaveTable)
    {
      ui->fatalError(Progmem::uiFeMemory);
      return false;
    }
  }

  if (interleaveTable) // not allocated on sequential sectors
  {
    memset(interleaveTable, 0, sectorsPerTrack+1);
    
    BYTE pos = 1;
    BYTE currentSector = 1;
    while (currentSector <= sectorsPerTrack)
    {
      interleaveTable[pos] = currentSector++;
      pos += interleave;
      if (pos > sectorsPerTrack)
      {
        pos = pos % sectorsPerTrack;
        while ((pos <= sectorsPerTrack) && (interleaveTable[pos]))
        {
          pos++;
        }
      }
    }
  }
  
  sramBeginBufferAccess(true, 0);
  for (BYTE sector = 0; sector < sectorsPerTrack; sector++)
  {
    if (badBlocksTable && badBlocksTable[sector])
    {
      sramWriteByteSequential(0x80); // mark bad block
    }
    else
    {
      sramWriteByteSequential(0); // mark good block
    }
    
    // set logical sector number from prepared interleave table
    sramWriteByteSequential(interleaveTable ? interleaveTable[sector+1] : sector+1);
  }  
  sramFinishBufferAccess();
  
  if (interleaveTable)
  {
    delete[] interleaveTable;
  }
  
  return true;
}

void WD42C22::formatTrack(BYTE sectorsPerTrack, WORD sectorSizeBytes, WORD* overrideCyl, BYTE* overrideHead)
{
  // expects the SRAM buffer already prepared with prepareFormatInterleave()  
  // overrideCyl, overrideHead: logical sector information differs from the physical cylinder and head
    
  // idPloLength: "length of the ID PLO sync field" - byte padding before the actual ID field starts,
  // for the Phase Locked Oscillator (in the data separator) to synchronize properly
  // ID PLO is programmable for a format command
  const BYTE idPloLength = 2;  // X + 9 => 2 + 9 = 11 byte padding for ID, see page 96 of the WD42C22A datasheet "SOFT SECTOR MFM/RLL FORMAT"
  // DATA PLO is fixed 11 bytes during format
  // byte data contents inside gaps and pads are set via loadParameterBlock()
  
  // auto-detect gap size
  // minimum format gap (GAP3) length formula found in one of the WD datasheets:
  // GAP3 = 2*M*S + K
  // where M is motor speed variation of disk, 0.03 for +- 3%;
  // S: sector length in bytes
  // K: 18 if no interleave (1:1), 0 if any interleave (always leaving 18 for a safe margin)  
  BYTE gapSize = sectorSizeBytes/16;
  gapSize += 18;
  
  BYTE bcr = adRead(0x37);
  BYTE icr = adRead(0x3B);
  
  icr &= 0xF7;
  adWrite(0x3B, icr);      // make sure MAC = 0 before changing DRWB    
  bcr |= 4;                // DRWB = 1
  adWrite(0x37, bcr);
  adWrite(0x34, 0);        // starting address of interleave table
  adWrite(0x35, 0);
  adWrite(0x3F, 0x40);     // ECCM = 0, DDRQ = 1
  icr |= 8;
  adWrite(0x3B, icr);      // MAC = 1  
  bcr |= 1;
  adWrite(0x37, bcr);      // ADBP = 1  
  icr &= 0xF7;
  adWrite(0x3B, icr);      // MAC = 0
  
  WORD currentCyl = m_physicalCylinder;
  BYTE currentHead = m_physicalHead;
  if (overrideCyl)
  {
    currentCyl = *overrideCyl;
  }
  if (overrideHead)
  {
    currentHead = *overrideHead;
  }
  
  // prepare task file registers
  adWrite(0x21, idPloLength);             // PLO length
  adWrite(0x22, sectorsPerTrack);         // sector count
  adWrite(0x23, gapSize-3);               // WD: Gap length written on disk is 3 bytes longer than gap value specified in sector number register
  adWrite(0x24, (BYTE)currentCyl);        // LSB
  adWrite(0x25, (BYTE)(currentCyl >> 8)); // MSB
  
  // prepare SDH register
  BYTE sdh = getSDHFromSectorSize(sectorSizeBytes);

  // ECC = 1 into SDH  
  if (m_params.DataVerifyMode != MODE_CRC_16BIT)
  {
    sdh |= 0x80;
  }
  sdh |= currentHead; // low 3 or 4 bits
  adWrite(0x26, sdh);
  
  m_result = WDC_OK;
  DWORD wait = 5000000UL;
  mcintFired = false;
  adWrite(0x27, 0x51);
  
  while (!mcintFired)
  {
    if (!--wait)
    {
      m_result = WDC_TIMEOUT;
      break;
    }
  }
  
  processResult();  
}

void WD42C22::writeSector(BYTE sectorNo, WORD sectorSizeBytes, WORD* overrideCyl, BYTE* overrideHead)
{
  // analog to readSector, just without "long mode"  
  // dataPloLength: byte padding of the data field; default 12 bytes + dataPloLength
  const BYTE dataPloLength = 0;
   
  BYTE bcr = adRead(0x37);
  BYTE icr = adRead(0x3B);
  
  icr &= 0xF7;
  adWrite(0x3B, icr);      // make sure MAC = 0 before changing DRWB    
  bcr |= 4;                // DRWB = 1
  adWrite(0x37, bcr);
  adWrite(0x34, 0);        // starting address of data in buffer
  adWrite(0x35, 0);
  adWrite(0x3F, 0x40);     // ECCM = 0, DDRQ = 1
  icr |= 8;
  adWrite(0x3B, icr);      // MAC = 1  
  bcr |= 1;
  adWrite(0x37, bcr);      // ADBP = 1  
  icr &= 0xF7;
  adWrite(0x3B, icr);      // MAC = 0
  
  WORD currentCyl = m_physicalCylinder;
  BYTE currentHead = m_physicalHead;
  if (overrideCyl)
  {
    currentCyl = *overrideCyl;
  }
  if (overrideHead)
  {
    currentHead = *overrideHead;
  }
  
  // prepare task file registers  
  adWrite(0x21, dataPloLength);           // PLO length
  adWrite(0x23, sectorNo);                // sector number
  adWrite(0x24, (BYTE)currentCyl);        // LSB
  adWrite(0x25, (BYTE)(currentCyl >> 8)); // MSB
  
  // prepare SDH register
  BYTE sdh = getSDHFromSectorSize(sectorSizeBytes);

  // ECC = 1 into SDH  
  if (m_params.DataVerifyMode != MODE_CRC_16BIT)
  {
    sdh |= 0x80;
  }
  sdh |= currentHead; // low 3 or 4 bits
  adWrite(0x26, sdh);

  m_result = WDC_OK;
  DWORD wait = 5000000UL;
  mcintFired = false;
  adWrite(0x27, 0x30); // write
  
  while (!mcintFired)
  {
    if (!--wait)
    {
      m_result = WDC_TIMEOUT;
      break;
    }
  }
  
  processResult();
}

void WD42C22::setBadSector(BYTE sectorNo, BYTE sectorsPerTrack, BYTE interleave, WORD sectorSizeBytes, WORD* overrideCyl, BYTE* overrideHead)
{
  // This makes use of the WD42C22 "Write ID command" to mark a bad sector without having to reformat the whole track:
  // as when we read or write an image, the first comes the sector numbering table, only then the actual data, where we verify their good/bad flag.
  
  // However, WD's Write ID command is not straightforward - it cannot modify an existing sector ID directly and easily.
  // This command was intended to append "special relocation ID fields" right after the normal ID; to remap bad sectors as a nonstandard feature.
  // A new sector ID field would be written IMMEDIATELY after a regular ID field, overlapping into the data of the old bad sector and destroying it.
  
  // So, WriteID is called with the F option set to F=1 (command byte 0xB8),
  // which requires an offset in bytes WHERE to write the sector ID with the new flag; instead of right after the old field.
  // As it cannot be zero, we supply the logical sector number of the *previous* sector, determined from an interleave table,
  // and as we know the sector gaps, then we can calculate the position of "our" sector.
  // With this offset in loadParameterBlock, the sector ID will be then written to the proper location.
  // loadParameterBlock will then need to be called again to restore normal behavior of the WDC.
  
  // The only problem with this usage of the WriteID command is that it won't mark the bad block flag reliably on the 1st sector of the track:
  // the gap between the last and first sector on track can be big, depending on the SPT count, and it can't be easily calculated.
  
  // Workaround for 1st sectors (sectorNo == 1) on track: use the "format single sector" command, normally supported on hard sectored media only.
  // Set it on the 1st sector, with the proper sector number and size, and set the W=1 flag (command byte 0xD3) - important!
  // After /INDEX (of the 1st soft-sector) and the re-creation of its ID field with the bad block flag on, WRITE GATE is turned off over gaps.
  // This is important as there's no SECTOR pulse occuring, normally required for this command, that would turn it off.
  // Otherwise the whole track would have been clobbered.
  
  // Summa summarum: use WriteID with F=1 for all sectors except the first,
  //                 use the hard sector FormatSingleSector with W=1 for the first sector, even though we're soft-sectored.
  // Anyone knows of a better solution, feel free to share...
  
  if (!sectorsPerTrack || !sectorSizeBytes)
  {
    return;
  }
     
  WORD currentCyl = m_physicalCylinder;
  BYTE currentHead = m_physicalHead;
  if (overrideCyl)
  {
    currentCyl = *overrideCyl;
  }
  if (overrideHead)
  {
    currentHead = *overrideHead;
  }
  
  // do WriteID with F=1
  if (sectorNo != 1)
  {
    // prepare 5 bytes sector ident
    sramBeginBufferAccess(true, 2043);    

    // determine the preceding sector from an interleave table; 1-based
    BYTE* interleaveTable = new BYTE[sectorsPerTrack + 1];
    if (!interleaveTable)
    {
      return;
    }
    memset(interleaveTable, 0, sectorsPerTrack+1);

    // fallback to sequential on invalid values
    if (!interleave || (interleave >= sectorsPerTrack))
    {
      interleave = 1;
    }

    BYTE pos = 1;
    BYTE currentSector = 1;
    while (currentSector <= sectorsPerTrack)
    {
      interleaveTable[pos] = currentSector++;
      pos += interleave;
      if (pos > sectorsPerTrack)
      {
        pos = pos % sectorsPerTrack;
        while ((pos <= sectorsPerTrack) && (interleaveTable[pos]))
        {
          pos++;
        }
      }
    }  
    
    // find the previous sector
    BYTE precedingSector = (BYTE)-1; 
    for (BYTE idx = 1; idx <= sectorsPerTrack; idx++)
    {
      if (interleaveTable[idx] == sectorNo)
      {
        precedingSector = interleaveTable[idx-1]; // the one before is not always minus one...
        break;
      }
    }
    delete[] interleaveTable;
    
    // cannot determine?
    if (precedingSector == (BYTE)-1)
    {
      return;
    }
    
    // BYTE0: sector number to find (before the byte offset, so sector preceding)
    sramWriteByteSequential(precedingSector);
    
    // BYTE1: IDENT (bits 7-4: one, bit 3: ~cyl10, bit 2: 1, bit 1: ~cyl9, bit 0: cyl8)
    const BYTE msb = (BYTE)(currentCyl >> 8);
    BYTE byte = 0xF4;
    if (!(msb & 4))
    {
      byte |= 8;
    }
    if (!(msb & 2))
    {
      byte |= 2;
    }
    if (msb & 1)
    {
      byte |= 1;
    }  
    sramWriteByteSequential(byte);
      
    // BYTE2: CYL LOW
    sramWriteByteSequential((BYTE)currentCyl);
    
    // BYTE3: HEAD (bit 7: bad block flag, 6-5: sector size like SDH, low 4 bits: head number)
    byte = getSDHFromSectorSize(sectorSizeBytes);
    byte |= currentHead | 0x80; // set BB=1
    sramWriteByteSequential(byte);
    
    // BYTE4: SEC# - logical sector number to write
    sramWriteByteSequential(sectorNo);   
    sramFinishBufferAccess(); // we're done with the buffer
    
    // now compute the proper offset and load it

    WORD offset = 3; // 2+1 bytes ID PAD, WRITE SPICE
    offset += sectorSizeBytes; // +data field size
    if (m_params.DataVerifyMode == MODE_CRC_16BIT)
    {
      offset += 2; // +2 bytes CRC
    }
    if (m_params.DataVerifyMode == MODE_ECC_32BIT)
    {
      offset += 4; // +4 bytes ECC
    }
    else if (m_params.DataVerifyMode == MODE_ECC_56BIT)
    {
      offset += 7; // +7 bytes ECC
    }  
    offset += 4; // +3 bytes DATA PAD, + 1 byte WS
    
    // +GAP, see formatTrack
    offset += sectorSizeBytes/16;
    offset += 18;
    
    // load offset to loadParameterBlock() -> use defaults from applyParams(), but set U=1 for writeID to work
    loadParameterBlock(m_params.UseRLL ? 0x33 : 0x4E, 0, true, offset);
    
    BYTE bcr = adRead(0x37);
    BYTE icr = adRead(0x3B);
    
    icr &= 0xF7;
    adWrite(0x3B, icr);      // make sure MAC = 0 before changing DRWB    
    bcr |= 4;                // DRWB = 1
    adWrite(0x37, bcr);
    adWrite(0x34, 0xFB);     // starting address of data in buffer - offset 2043
    adWrite(0x35, 7);
    adWrite(0x3F, 0x40);     // ECCM = 0, DDRQ = 1
    icr |= 8;
    adWrite(0x3B, icr);      // MAC = 1  
    bcr |= 1;
    adWrite(0x37, bcr);      // ADBP = 1  
    icr &= 0xF7;
    adWrite(0x3B, icr);      // MAC = 0
    
    // prepare task file registers  
    adWrite(0x21, 1);                       // set PLO length to 1 as a zero would cause a 2048-byte PLO field here...
    adWrite(0x22, 1);                       // sector count
    adWrite(0x23, sectorNo);                // sector number
    adWrite(0x24, (BYTE)currentCyl);        // cyl LSB
    adWrite(0x25, (BYTE)(currentCyl >> 8)); // cyl MSB
    
    // prepare SDH register
    BYTE sdh = getSDHFromSectorSize(sectorSizeBytes);

    // ECC = 1 into SDH  
    if (m_params.DataVerifyMode != MODE_CRC_16BIT)
    {
      sdh |= 0x80;
    }
    sdh |= currentHead; // low 3 or 4 bits
    adWrite(0x26, sdh);
    
    m_result = WDC_OK;
    DWORD wait = 5000000UL;
    mcintFired = false;
    adWrite(0x27, 0xB8); // write ID
    
    while (!mcintFired)
    {
      if (!--wait)
      {
        m_result = WDC_TIMEOUT;
        break;
      }
    }
    
    processResult();
    
    // set U back to 0 to disable non-standard sector sizes
    const BYTE saveResult = m_result;
    const BYTE saveMessage = m_errorMessage;
    
    loadParameterBlock(m_params.UseRLL ? 0x33 : 0x4E, 0);
    
    m_result = saveResult;
    m_errorMessage = saveMessage; 
  }
  
  // do format single sector after INDEX with W=1
  else
  {
    // see formatTrack
    const BYTE idPloLength = 2;
    BYTE gapSize = sectorSizeBytes/16;
    gapSize += 18;
  
    // ditto, just for one sector
    sramBeginBufferAccess(true, 0);
    sramWriteByteSequential(0x80);
    sramWriteByteSequential(sectorNo);
    sramFinishBufferAccess();
    
    BYTE bcr = adRead(0x37);
    BYTE icr = adRead(0x3B);
    
    icr &= 0xF7;
    adWrite(0x3B, icr);      // make sure MAC = 0 before changing DRWB    
    bcr |= 4;                // DRWB = 1
    adWrite(0x37, bcr);
    adWrite(0x34, 0);        // starting address of interleave table
    adWrite(0x35, 0);
    adWrite(0x3F, 0x40);     // ECCM = 0, DDRQ = 1
    icr |= 8;
    adWrite(0x3B, icr);      // MAC = 1  
    bcr |= 1;
    adWrite(0x37, bcr);      // ADBP = 1  
    icr &= 0xF7;
    adWrite(0x3B, icr);      // MAC = 0    
       
    // prepare task file registers
    adWrite(0x21, idPloLength);             // PLO length
    adWrite(0x22, sectorNo);                // first sector
    adWrite(0x23, gapSize-3);               // WD: Gap length written on disk is 3 bytes longer than gap value specified in sector number register
    adWrite(0x24, (BYTE)currentCyl);        // LSB
    adWrite(0x25, (BYTE)(currentCyl >> 8)); // MSB
    
    // prepare SDH register
    BYTE sdh = getSDHFromSectorSize(sectorSizeBytes);

    // ECC = 1 into SDH  
    if (m_params.DataVerifyMode != MODE_CRC_16BIT)
    {
      sdh |= 0x80;
    }
    sdh |= currentHead; // low 3 or 4 bits
    adWrite(0x26, sdh);
    
    m_result = WDC_OK;
    DWORD wait = 5000000UL;
    mcintFired = false;
    adWrite(0x27, 0xD3); // format single sector, W=1
    
    while (!mcintFired)
    {
      if (!--wait)
      {
        m_result = WDC_TIMEOUT;
        break;
      }
    }
    
    processResult();
  }
  
}
