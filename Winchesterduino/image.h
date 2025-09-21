// Winchesterduino (c) 2025 J. Bogin, http://boginjr.com
// WDI disk imaging

#pragma once

#define SD_READ(bytes)  res = doImageFile.read(doBuffer, (bytes)); \
                        if (res == -1) { doProgmemResponseStr = Progmem::imgXferCardError; return false; } \
                        else if (res < (bytes)) { doProgmemResponseStr = Progmem::imgXferErrEnd; return false; } \                        
                       
#define SD_WRITE(bytes) if (doImageFile.write(doBuffer, (bytes)) == 0) { doProgmemResponseStr = Progmem::imgXferCardError; return false; }

void CommandReadImage();
void CommandWriteImage();

