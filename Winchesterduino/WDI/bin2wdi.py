# Winchesterduino (c) 2025 J. Bogin, http://boginjr.com
# Binary image to WDI converter

# Syntax: python bin2wdi.py input.img output.wdi

import sys

from wdi.writer import WdiWriter

def main():
    if (len(sys.argv) != 3):
        print("Converts a raw disk image into the Winchesterduino WDI format.\n\nbin2wdi.py input.img output.wdi");
        return
    
    wdi = WdiWriter()
    
    print("\nCurrent limits of this binary conversion script:\n")
    print(" - all sector counts, sizes and interleave considered constant throughout the whole drive,")
    print(" - logical cylinder and head numbers in sector ID fields equal to their physical address,")
    print(" - logical sectors are always numbered from one,")
    print(" - no indication of bad blocks, CRC/ECC errors or unreadable tracks.\n")
    
    if (wdi.askKey("Continue? (Y/N): ", "YN") == 'N'):
        return
    print("")
    
    wdi.initialize(sys.argv[1], sys.argv[2])
    if (not wdi.isInitialized()):
        print("Cannot open supplied file(s)")
        return
    
    key = 0
    params = {}
    print("Press Ctrl+C to abort.\n\nEnter disk drive parameters for the output image:\n")
    
    # WDI output image parameters
    
    params["dataMode"] = 0 if wdi.askKey("Data separator (M)FM / (R)LL: ", "MR") == 'M' else 1
    if (params["dataMode"] == 0):
        key = wdi.askKey("Data verify mode (1)6bit CRC / (3)2bit ECC / (5)6bit ECC: ", "135")
        if (key == '1'):
            params["dataVerify"] = 0
        elif (key == '3'):
            params["dataVerify"] = 1
        else:
            params["dataVerify"] = 2
    else: # RLL always 56-bit ECC
        params["dataVerify"] = 2
    
    params["cylinders"] = wdi.askRange("Cylinders (1-2048): ", 1, 2048)
    params["heads"] = wdi.askRange("Heads (1-16): ", 1, 16)
    
    # reduced write current
    if (params["heads"] <= 8):
        params["rwcEnabled"] = 1 if wdi.askKey("Drive requires reduced write current? Y/N: ", "YN") == 'Y' else 0
        if (params["rwcEnabled"] == 1):
            params["rwcStartCylinder"] = wdi.askRange("Reduced write current start cylinder (0: always): ", 0, 2048)
        else:
            params["rwcStartCylinder"] = 0
    else:
        params["rwcEnabled"] = 0
        params["rwcStartCylinder"] = 0
        
    params["wpEnabled"] = 1 if wdi.askKey("Drive requires write precompensation? Y/N: ", "YN") == 'Y' else 0
    if (params["wpEnabled"] == 1):
        params["wpStartCylinder"] = wdi.askRange("Write precompensation start cylinder (0: always): ", 0, 2048)
    else:
        params["wpStartCylinder"] = 0
        
    params["lzEnabled"] = 1 if wdi.askKey("Is landing zone specified (no auto parking)? Y/N: ", "YN") == 'Y' else 0
    if (params["lzEnabled"] == 1):
        params["lzStartCylinder"] = wdi.askRange("Landing zone (parking cylinder): ", 0, 2048)
    else:
        params["lzStartCylinder"] = 0
        
    params["seekType"] = 1 if wdi.askKey("Drive seeking: (B)uffered / (S)T-506 compatible: ", "BS") == 'S' else 0
    
    params["description"] = ""
    print("\nOptional text description of this image, empty line quits:")
    description = input("").strip()
    while (len(description)):
        params["description"] += description + "\r\n"
        description = input("").strip()        
    
    # input image parameters
    print("Enter input binary image parameters:\n")
    params["spt"] = wdi.askRange("Sectors per track (1-63): ", 1, 63)
    
    key = wdi.askKey("Sector size (1)28B (2)56B (5)12B 1(K)B: ", "125K")
    if (key == '1'):
        params["sdh"] = 0x60
        params["ssize"] = 128
    elif (key == '2'):
        params["sdh"] = 0
        params["ssize"] = 256
    elif (key == '5'):
        params["sdh"] = 0x20
        params["ssize"] = 512
    else:
        params["sdh"] = 0x40
        params["ssize"] = 1024
    
    params["interleave"] = wdi.askRange("Sector interleave (1: none): ", 1, params["spt"])
    
    print("")    
    expectedSize = params["cylinders"] * params["heads"] * params["spt"] * params["ssize"]
    actualSize = wdi.getInputFileSize()
    
    if (expectedSize != actualSize):
        print("Expected binary image size of " + str(params["cylinders"]) + " cyl(s), " + str(params["heads"])
              + " head(s), " + str(params["spt"]) + " sector(s) per track, " + str(params["ssize"]) + "B each:")
        print(str(expectedSize) + " bytes")    
        print("Actual size:\n" + str(actualSize) + " bytes\n")
    
        if (actualSize < (params["ssize"] * params["spt"])):
            print("Really?")
            return
            
        if (wdi.askKey("Continue? (Y/N): ", "YN") == 'N'):
            return
        print("")
            
    # write WDI
    print("Processing...")
    if (wdi.writeHeader(params)):
        cylsProcessed = wdi.writeData(params)
        print(str(cylsProcessed) + " cylinder(s) written.")
    
if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        sys.exit(-1)
        