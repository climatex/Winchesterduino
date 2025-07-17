# Winchesterduino (c) 2025 J. Bogin, http://boginjr.com
# WDI disk image inspector

# Syntax: python inspect.py image.wdi [-t] [-e] [-b image.img] [-1]
#         -t: print detailed sector ID information for each track,
#         -e: print detailed parse error information, if any,
#         -b: also save a raw binary disk image (-1: reinterleave to 1:1).

import sys

from wdi.parser import WdiParser

def main():   
    argc = len(sys.argv)
    if ((argc < 2) or (argc > 6)):
        showUsage()
        return
        
    verboseErrors = None
    verboseTrackListing = None
    binaryOutputFileName = None
    binaryOutputReinterleave = False
    idx = 2
    while (idx < argc):
        if (sys.argv[idx] == "-1"):
            binaryOutputReinterleave = True
        elif ((verboseErrors is None) and (sys.argv[idx].lower() == "-e")):
            verboseErrors = True        
        elif ((verboseTrackListing is None) and (sys.argv[idx].lower() == "-t")):
            verboseTrackListing = True
        elif (binaryOutputFileName is None) and (((sys.argv[idx].lower() == "-b") and (argc > idx+1))):
            binaryOutputFileName = sys.argv[idx+1]
        elif ((sys.argv[idx-1]).lower() != "-b"):
            showUsage()
            return
        idx += 1
    
    if (sys.argv[1] == binaryOutputFileName):
        print("Really?")
        return
    
    wdi = WdiParser(sys.argv[1], verboseErrors, verboseTrackListing, binaryOutputFileName, binaryOutputReinterleave)
    if (not wdi.isInitialized()):
        print("Cannot open supplied file(s).")
        return
      
    params = wdi.getImageParams()
    if (params["result"] == False):
        print("Invalid WDI file specified.")
        return
      
    if (showDriveParameters(params) == False): 
        return
        
    parse = wdi.parse()
    if (parse["result"] == False):
        print("Track/sector data fields contain invalid or incomplete values (was transfer aborted?)")
        if (verboseErrors is None):
            print("Use the -e command line argument to display error details.")
        return
        
    print("Disk image checks passed. The hard drive contained:")
    print(str(parse["badBlocks"]) + " bad block(s),")
    print(str(parse["unreadableTracks"]) + " unreadable track(s),") 
    print(str(parse["dataErrors"]) + " CRC/ECC error(s).") 
    if (verboseTrackListing is None):
        print("Specify the -t command line argument to display detailed sector layout of each track.")
    if (binaryOutputFileName is None):
        print("You can also use the -b argument to generate a binary (raw) disk image.")
    else:
        print("Creating raw disk image", "(original interleave)." if not binaryOutputReinterleave else "(reinterleave to 1:1).")        
    
    print("\nProcessing done")
    return

def showUsage():
    print("Inspects a Winchesterduino disk image.\n\ninspect.py image.wdi [-t] [-e] [-b image.img] [-1]\n");
    print("  -t\t\tPrint detailed sector ID information per track.")
    print("  -e\t\tPrint detailed parse error information, if any.")
    print("  -b image.img\tCreates a binary (raw) disk image.")
    print("  -1\t\tReorder sectors in the binary image to 1:1 interleave.")
    return
      
def showDriveParameters(params):
    errors = ""
    if ((params["dataMode"] < 0) or (params["dataMode"] > 1)):
        errors += "\nData encoding mode must be 0 or 1"
    if ((params["dataVerify"] < 0) or (params["dataVerify"] > 2)):
        errors += "\nData verify mode must be 0 to 2"
    if ((params["cylinders"] < 1) or (params["cylinders"] > 2048)):
        errors += "\nNumber of cylinders must be 1 to 2048"
    if ((params["heads"] < 1) or (params["heads"] > 16)):
        errors += "\nNumber of heads must be 1 to 16"
    if ((params["wpStartCylinder"] < 0) or (params["wpStartCylinder"] > 2047)):
        errors += "\nWrite precomp start cylinder must be within 0 to 2047"
    if ((params["rwcStartCylinder"] < 0) or (params["rwcStartCylinder"] > 2047)):
        errors += "\nRWC start cylinder must be within 0 to 2047"
    if ((params["lzStartCylinder"] < 0) or (params["lzStartCylinder"] > 2047)):
        errors += "\nLanding zone start cylinder must be within 0 to 2047"
        
    if (len(errors) > 0):
        print("Invalid disk drive parameters detected in WDI file:")
        print(errors)
        return False
        
    if (len(params["description"]) > 0):
        print("Disk image description:")
        print(params["description"])
        
    print("Drive parameters:\n")
    temp = "MFM" if params["dataMode"] == 0 else "RLL"
    print("Data separator mode:\t" + temp)
    print("Data verify mode:\t", end="")
    if (params["dataVerify"] == 0):
        print("16-bit CRC")
    elif (params["dataVerify"] == 1):
        print("32-bit ECC")
    else:
        print("56-bit ECC")
    print("Cylinders:\t\t" + str(params["cylinders"]))
    print("Heads:\t\t\t" + str(params["heads"]))
    temp = "disabled" if params["rwcEnabled"] == 0 else "enabled, from cylinder " + str(params["rwcStartCylinder"])
    print("Reduced write current:\t" + temp)
    temp = "disabled" if params["wpEnabled"] == 0 else "enabled, from cylinder " + str(params["wpStartCylinder"])
    print("Write precompensation:\t" + temp)
    temp = "yes" if params["lzEnabled"] == 0 else "no"
    print("Autopark on powerdown:\t" + temp)
    if (params["lzEnabled"] != 0):
        print("Landing zone cylinder:\t" + str(params["lzStartCylinder"]))
    temp = "fast, buffered" if params["seekType"] == 0 else "slow, ST-506 compatible"
    print("Drive seeking mode:\t" + temp)
    print("")
        
    return True
    
if __name__ == "__main__":
    main()