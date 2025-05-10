// Winchesterduino (c) 2025 J. Bogin, http://boginjr.com
// WDI disk imaging

#pragma once

// ask for the next data packet?
#define CHECK_STREAM_END if (packetIdx >= size) return true;

void CommandReadImage();
void CommandWriteImage();

bool IsSerialTransfer();
void DumpSerialTransfer();

