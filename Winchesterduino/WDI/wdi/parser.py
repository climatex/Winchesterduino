# Winchesterduino (c) 2025 J. Bogin, http://boginjr.com
# WDI file parser

import io

class WdiParser:
    def __init__(self, wdiFileName,
                       verboseErrors = None, 
                       verboseTrackListing = None,
                       binaryOutputFileName = None,
                       binaryOutputReinterleave = False,
                       badBlockFillByte = 0):
                
        # all files opened OK
        self._initialized = False
        
        # binary output: what to fill sectors with no address marks or bad block flags
        # sectors with CRC/ECC errors are dumped as they are
        self._badBlockFillByte = badBlockFillByte
        
        # binary output: write logical sectors in original interleave, or reorder to 1:1 interleave
        self._binaryOutputReinterleave = binaryOutputReinterleave
        
        if (verboseErrors is None):
            self._verboseErrors = False
        else:
            self._verboseErrors = verboseErrors
        if (verboseTrackListing is None):
            self._verboseTrackListing = False
        else:
            self._verboseTrackListing = verboseTrackListing
        if (binaryOutputFileName is None):
            self._binaryOutput = None
            
        # open files
        try:
            self._file = open(wdiFileName, "rb")
            if (binaryOutputFileName is not None):
                self._binaryOutput = open(binaryOutputFileName, "wb")
                self._binaryOutput.seek(0)
            self._initialized = True
        except:
            pass
            
    def __del__(self):
        if (self._initialized):
            try:
                self._file.close()
                if (self._binaryOutput is not None):
                    self._binaryOutput.close()
            except:
                pass
                
    def isInitialized(self):
        return self._initialized
                
    def wasEndOfFile(self):
        currPos = self._file.tell()
        self._file.seek(0, 2)
        endPos = self._file.tell()
        self._file.seek(currPos, 0)
        return (currPos >= endPos)
        
    def verifyHeader(self):
        if (not self._initialized):
            return False
            
        # verify text header (and optional description) presence
        self._file.seek(0)
        signature = self._file.read(4)
        if (signature != b'WDI '):
            return False
        while True:
            byte = self._file.read(1)
            if (not byte):
                return False
            elif (byte[0] == 0x1A):
                return True
                
    def getUserDescription(self):
        if (not self.verifyHeader()):
            return ""            
        
        # skip signature
        self._file.seek(0)
        self._file.read(51)
        
        description = []
        byte = self._file.read(1)
        while (byte and (byte[0] != 0x1A)):
            description.append(byte[0])
            byte = self._file.read(1)
            
        return "" if not description else bytearray(description).decode("ascii")
        
    def getImageParams(self):
        if (not self.verifyHeader()):
            return {"result": False}            
        
        # description, if any
        description = self.getUserDescription()
        startPos = self._file.tell()
        
        # verify length
        self._file.read(32)
        if (self.wasEndOfFile()):
            return {"result": False}
        self._file.seek(startPos)
        
        # data mode (MFM/RLL) and data verify (16-bit CRC, 32-bit ECC, 56-bit ECC)
        dataMode = self._file.read(1)[0]
        dataVerify = self._file.read(1)[0]
        
        # total physical cylinders
        phcyl_lsb = self._file.read(1)[0]
        phcyl_msb = self._file.read(1)[0]
        phcyl = (phcyl_msb << 8) | phcyl_lsb
        
        # total physical heads
        heads = self._file.read(1)[0]
        
        # write precompensation enabled flag, and starting cylinder
        wpEnabled = self._file.read(1)[0]
        wpcyl_lsb = self._file.read(1)[0]
        wpcyl_msb = self._file.read(1)[0]
        wpcyl = (wpcyl_msb << 8) | wpcyl_lsb
        
        # reduced write current enabled flag, and starting cylinder
        rwcEnabled = self._file.read(1)[0]
        rwccyl_lsb = self._file.read(1)[0]
        rwccyl_msb = self._file.read(1)[0]
        rwccyl = (rwccyl_msb << 8) | rwccyl_lsb
        
        # landing zone
        lzEnabled = self._file.read(1)[0]
        lzcyl_lsb = self._file.read(1)[0]
        lzcyl_msb = self._file.read(1)[0]
        lzcyl = (lzcyl_msb << 8) | lzcyl_lsb
        
        # seek type (buffered / ST506)
        seekType = self._file.read(1)[0]
        
        # does the image not contain all cylinders?
        partialImage = self._file.read(1)[0]
        partialImageStartCyl_lsb = self._file.read(1)[0]
        partialImageStartCyl_msb = self._file.read(1)[0]
        partialImageStartCyl = (partialImageStartCyl_msb << 8) | partialImageStartCyl_lsb
        partialImageEndCyl_lsb = self._file.read(1)[0]
        partialImageEndCyl_msb = self._file.read(1)[0]
        partialImageEndCyl = (partialImageEndCyl_msb << 8) | partialImageEndCyl_lsb
        
        # read the padded rest to begin on first data field
        self._file.read(12)
        
        return {"result": True,
                "description": description,
                "dataMode": dataMode,
                "dataVerify": dataVerify,
                "cylinders": phcyl,
                "heads": heads,
                "wpEnabled": wpEnabled,
                "wpStartCylinder": wpcyl,
                "rwcEnabled": rwcEnabled,
                "rwcStartCylinder": rwccyl,
                "lzEnabled": lzEnabled,
                "lzStartCylinder": lzcyl,
                "seekType": seekType,
                "partialImage": partialImage,
                "partialImageStartCylinder": partialImageStartCyl,
                "partialImageEndCylinder": partialImageEndCyl}
                            
    def sdhToSectorSize(self, sdh):
        test = sdh & 0x60;
        if (test == 0x60):
            return 128;
        elif (test == 0x40):
            return 1024;
        elif (test == 0x20):
            return 512;
        else:
            return 256;
    
    def getInterleave(self, sectorMap):
        if (len(sectorMap) == 0):
            return None
        elif (len(sectorMap) < 3):
            return "1:1"
        try:
            interleave = sectorMap.index(sectorMap[0]+1)
            verify = interleave
            for idx in range(len(sectorMap)):
                if ((len(sectorMap)-(idx+1)) > interleave):
                    verify = sectorMap[idx+1:].index(sectorMap[idx+1]+1)
                    if (verify != interleave):
                        break
            return (str(interleave) + ":1") if (interleave == verify) else "unknown"                
        except:
            pass
        return "unknown"
    
    def parse(self):
    #
        params = self.getImageParams()
        if (params["result"] == False):
            return {"result": False}

        unreadableTracks = 0
        badBlocks = 0
        dataErrors = 0

        while True:
        #  
            # read current physical cylinder            
            phcyl_lsb = self._file.read(1)
            if (not phcyl_lsb):
            #
                if (self.wasEndOfFile()):
                #
                    return {"result": True, 
                            "unreadableTracks": unreadableTracks,
                            "badBlocks": badBlocks,
                            "dataErrors": dataErrors}
                #
                else:
                #
                    if (self._verboseErrors):
                        print("Expected physical cylinder LSB, got end-of-file at offset", 
                               hex(self._file.tell()))
                    return {"result": False}
                #
            #    
          
            phcyl_msb = self._file.read(1)
            if (not phcyl_msb):
            #
                # XMODEM end-of-file
                if (self.wasEndOfFile()) and (phcyl_lsb[0] == 0x1A):
                #
                    return {"result": True, 
                            "unreadableTracks": unreadableTracks,
                            "badBlocks": badBlocks,
                            "dataErrors": dataErrors}
                #
                if (self._verboseErrors):
                    print("Expected physical cylinder MSB, got end-of-file at offset", 
                          hex(self._file.tell()))
                return {"result": False}
            #
          
            # XMODEM end-of-file
            if (phcyl_msb[0] == 0x1A):
            #
                return {"result": True, 
                              "unreadableTracks": unreadableTracks,
                              "badBlocks": badBlocks,
                              "dataErrors": dataErrors}
            #
            
            phcyl = (phcyl_msb[0] << 8) | phcyl_lsb[0]
            if (phcyl > 2047):
            #
                if (self._verboseErrors):
                    print("Invalid physical cylinder value " + str(phcyl) + ", must be 0-2047, at offset",
                         hex(self._file.tell()-1))
                return {"result": False}
            #
            
            # current head          
            phhead = self._file.read(1)
            if (not phhead):
            #
                if (self._verboseErrors):
                    print("Expected physical head byte, got end-of-file at offset",
                          hex(self._file.tell()))
                return {"result": False}
            #
            if (phhead[0] > 15):
            #
                if (self._verboseErrors):
                    print("Invalid physical head value " + str(phhead[0]) + ", must be 0-15 at offset",
                          hex(self._file.tell()-1))
                return {"result": False}
            #
            
            # sectors per track in current track
            spt = self._file.read(1)
            if (not spt):
            #
                if (self._verboseErrors):
                    print("Expected sectors per track, got end-of-file at offset",
                          hex(self._file.tell()))
                return {"result": False}
            #
            if (spt[0] > 64):
            #
                if (self._verboseErrors):
                    print("Invalid sectors per track value " + str(spt[0]) + ", must be 0-64 at offset",
                          hex(self._file.tell()-1))
                return {"result": False}
            #
            
            # print out track structure
            if (self._verboseTrackListing):
                print("Cylinder:", phcyl, "Head:", phhead[0], "-> ", end="")      
            if (spt[0] == 0):
            #
                if (self._verboseTrackListing):
                    print("track unreadable (no sector IDs)\n")
                  
                unreadableTracks += 1
                continue
            #
            if (self._verboseTrackListing):
                print(spt[0], "sectors per track")

            logcyls = []
            logsdhs = [] 
            logsectors = []   
            
            # sector numbering map (interleave table)
            currSector = 0    
            while (currSector < spt[0]):
            #
                currSector += 1
                
                # logical cylinder number
                logcyl_lsb = self._file.read(1)
                if (not logcyl_lsb):
                #
                    if (self._verboseErrors):
                        print("Expected logical cylinder LSB, got end-of-file at offset",
                              hex(self._file.tell()))
                    return {"result": False}
                #
                
                logcyl_msb = self._file.read(1)
                if (not logcyl_msb):
                #
                    if (self._verboseErrors):
                        print("Expected logical cylinder MSB, got end-of-file at offset",
                              hex(self._file.tell()))
                    return {"result": False}
                #
                logcyls.append((logcyl_msb[0] << 8) | logcyl_lsb[0])
                
                # logical sector number
                logsector = self._file.read(1)
                if (not logsector):
                #
                    if (self._verboseErrors):
                        print("Expected logical sector byte, got end-of-file at offset",
                              hex(self._file.tell()))
                    return {"result": False}
                #
                logsectors.append(logsector[0])
                
                # SDH byte
                logsdh = self._file.read(1)
                if (not logsdh):
                #
                    if (self._verboseErrors):
                        print("Expected SDH byte, got end-of-file at offset",
                              hex(self._file.tell()))
                    return {"result": False}
                #      
                logsdhs.append(logsdh[0])
            #
            
            if (self._verboseTrackListing):
            #
                print("Logical cylinder numbers: ", end="")
                print(logcyls)                
                print("SDH bytes: [", end="")
                if (logsdhs):
                    sdhs = "".join("0x%02X, " % b for b in logsdhs)[:-2]
                    print(sdhs + "]")
                else:
                    print("]")
                print("Logical sectors: ", end="")
                print(logsectors, end="")
                if (logsectors):
                    interleave = self.getInterleave(logsectors)
                    if (interleave is not None):
                        print(" (" + interleave + " interleave)", end="")
                print("")                
            #
            
            # binary output data: [ (logicalSectorNo,data), (logicalSectorNo,data) ...]
            #                           1st physical sector          2nd
            outputData = []
            
            # sector data record
            currSector = 0
            while (currSector < spt[0]):      
            # 
                currSector += 1
                sectorSizeBytes = self.sdhToSectorSize(logsdhs[currSector-1])                                
                
                # sector data type
                datatype = self._file.read(1)
                if (not datatype):
                #
                    if (self._verboseErrors):
                        print("Expected sector data type, got end-of-file at offset",
                              hex(self._file.tell()))
                    return {"result": False}
                #
                
                if ((datatype[0] & 0x7F) > 2):
                #
                    if (self._verboseErrors):
                        print("Invalid sector data type " + str(hex(datatype[0])) + ", must be 0-2 or 0x81-0x82 at offset",
                              hex(self._file.tell()-1))      
                    return {"result": False}
                #
                
                if (datatype[0] == 0):
                #
                    badBlocks += 1
                    if (self._verboseTrackListing):
                        print("Sector", logsectors[currSector-1], ": Bad block")
                    
                    # write bad block fill
                    if (self._binaryOutput is not None):
                        outputData.append( (logsectors[currSector-1], bytes([self._badBlockFillByte]*sectorSizeBytes)) )
                    
                    continue
                #
                elif (datatype[0] == 2):
                #
                    dataErrors += 1
                    if (self._verboseTrackListing):
                        print("Sector", logsectors[currSector-1], ": CRC/ECC data error")
                #
            
                # sector contents                
                if (datatype[0] & 0x80):
                #
                    compressedData = self._file.read(1)
                    if (not compressedData):
                    #
                        if (self._verboseErrors):
                            print("Expected compressed sector data, got end-of-file at offset",
                                  hex(self._file.tell()))
                        return {"result": False}
                    #
                        
                    if (self._binaryOutput is not None):
                        outputData.append( (logsectors[currSector-1], bytes([compressedData[0]]*sectorSizeBytes)) )
                #
                else:
                #
                    sectorData = self._file.read(sectorSizeBytes)
                    if ((not sectorData) or (len(sectorData) < sectorSizeBytes)):
                    #
                        if (self._verboseErrors):
                            print("Expected " + str(sectorSizeBytes) + "B sector data, got end-of-file at offset",
                                  hex(self._file.tell()))
                        return {"result": False}
                    #
                    
                    if (self._binaryOutput is not None):
                        outputData.append( (logsectors[currSector-1], sectorData) )
                #
            #
            
            if (self._verboseTrackListing):
                print("")
            
            # write binary output file
            if (outputData):
            #
                # as-is
                if (not self._binaryOutputReinterleave):
                #
                    for (logicalSectorNo, data) in outputData:
                    #
                        try:
                            self._binaryOutput.write(data)
                        except:
                        #
                            print("Error writing binary disk image")
                            return {"result": False}
                        # 
                    #
                #
                
                # reinterleave to 1:1
                else:
                #
                    logsectors.sort()
                    
                    for logicalSectorNo in logsectors:
                    #
                        data = next((item[1] for item in outputData if item[0] == logicalSectorNo), None)
                        if (data is not None):
                        #
                            try:
                                self._binaryOutput.write(data)                              
                            except:
                            #
                                print("Error writing binary disk image")
                                return {"result": False}
                            #                            
                        #
                    #
                #
            #
        #
    #

if __name__ == "__main__":
    print("Not to be executed manually, use the scripts from one level up")
   