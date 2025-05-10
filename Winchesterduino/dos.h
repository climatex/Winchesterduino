// Winchesterduino (c) 2025 J. Bogin, http://boginjr.com
// DOS filesystem viewer

WORD  DOSGetSectorSize();
DWORD DOSGetTotalSectorCount();
void  DOSConvertLogicalSectorToCHS(const DWORD& logical, WORD& cylinder, BYTE& head, BYTE& sector);

void  CommandDos();