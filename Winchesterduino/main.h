// Winchesterduino (c) 2025 J. Bogin, http://boginjr.com
// Main functions

#pragma once
#include "config.h"

void InitializeDisk();
void MainLoop();

// helpers
DWORD* CalculateSectorsPerTrack(BYTE sdh,
                                BYTE& sectorsPerTrack, WORD& tableCount,
                                bool& headMismatch, bool& cylinderMismatch, bool& variableSectorSize);

bool CalculateInterleave(const DWORD* sectorsTable, WORD tableCount, BYTE sectorsPerTrack, BYTE& result);