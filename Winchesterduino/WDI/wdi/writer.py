# Winchesterduino (c) 2025 J. Bogin, http://boginjr.com
# WDI file writer

import io

class WdiWriter:
    def __init__(self):
        self._initialized = False
            
    def __del__(self):
        if (self._initialized):
            try:
                self._inputFile.close()
                self._outputFile.close()
            except:
                pass
    
    def initialize(self, binFileName, wdiFileName):
        try:
            self._inputFile = open(binFileName, "rb")
            self._outputFile = open(wdiFileName, "wb")
            self._inputFile.seek(0)
            self._outputFile.seek(0)
            self._initialized = True
        except:
            pass
            
    def isInitialized(self):
        return self._initialized
                
    def askKey(self, prompt, keys=""):
        while True:
            result = input(prompt).strip()
            if (len(keys) == 0):
                return result
            if (len(result) != 1):
                continue
            result = result.upper()
            keys = keys.upper()
            idx = 0
            while (idx < len(keys)):
                if (result[0] == keys[idx]):
                    return result[0]
                idx += 1
                
    def askRange(self, prompt, min, max):
        while True:
            result = input(prompt).strip()
            if (len(result) == 0):
                continue
            num = int(result)
            if ((num >= min) and (num <= max)):
                return num
    
    def getInputFileSize(self):
        if (self._initialized):
            pos = self._inputFile.tell()
            self._inputFile.seek(0, 2)
            size = self._inputFile.tell()
            self._inputFile.seek(pos)
            return size
        return 0
                
    def writeHeader(self, params):
        try:
            
            # signature and description
            self._outputFile.write(b"WDI file created by Winchesterduino, (c) J. Bogin\r\n")
            description = params["description"].strip()
            if (len(description) > 0):
                self._outputFile.write(bytes(description, "ascii", "replace"))
                self._outputFile.write(bytes([0x0D]))
                self._outputFile.write(bytes([0x0A]))        
            self._outputFile.write(bytes([0x1A]))
            
            # drive parameters table
            self._outputFile.write(bytes([params["dataMode"]]))
            self._outputFile.write(bytes([params["dataVerify"]]))
            self._outputFile.write(bytes([params["cylinders"] & 0xFF]))
            self._outputFile.write(bytes([params["cylinders"] >> 8]))
            self._outputFile.write(bytes([params["heads"]]))
            self._outputFile.write(bytes([params["wpEnabled"]]))
            self._outputFile.write(bytes([params["wpStartCylinder"] & 0xFF]))
            self._outputFile.write(bytes([params["wpStartCylinder"] >> 8]))
            self._outputFile.write(bytes([params["rwcEnabled"]]))
            self._outputFile.write(bytes([params["rwcStartCylinder"] & 0xFF]))
            self._outputFile.write(bytes([params["rwcStartCylinder"] >> 8]))
            self._outputFile.write(bytes([params["lzEnabled"]]))
            self._outputFile.write(bytes([params["lzStartCylinder"] & 0xFF]))
            self._outputFile.write(bytes([params["lzStartCylinder"] >> 8]))
            self._outputFile.write(bytes([params["seekType"]]))
            self._outputFile.write(bytes([0]*17))
            return True
        
        except:
            print("File write error")            
            return False
            
    def writeData(self, params):
        cyl = 0
        head = 0
        sourceInterleave = self.getInterleaveTable(params, params["sourceInterleave"])
        targetInterleave = self.getInterleaveTable(params, params["targetInterleave"])
        seekPos = 0
        inputSize = self.getInputFileSize()
        
        while (seekPos < inputSize):
        
            # reinterleave source
            sectors = []
            tableIndex = 1
            while (tableIndex <= params["spt"]):
                sector = self._inputFile.read(params["ssize"])
                if ((not sector) or (len(sector) < params["ssize"])):
                    return cyl                
                sectors.append( (sourceInterleave[tableIndex], sector) )
                seekPos += params["ssize"]
                tableIndex += 1
                
            # reinterleave to target
            data = bytearray()
            tableIndex = 1
            while (tableIndex <= params["spt"]):
                found = next((item[1] for item in sectors if item[0] == targetInterleave[tableIndex]), None)
                if (found is not None):
                    data.extend(found)
                tableIndex += 1           
                
            try:           
                # write track data field
                self._outputFile.write(bytes([cyl & 0xFF]))
                self._outputFile.write(bytes([cyl >> 8]))
                self._outputFile.write(bytes([head]))
                self._outputFile.write(bytes([params["spt"]]))
                
                # sector numbering map
                sectorMap = bytearray([0]*4*params["spt"])
                pos = 0
                ofs = 0
                while (pos < params["spt"]):
                    logicalSectorNo = targetInterleave[pos+1]
                    
                    # needs to be adjusted?
                    if (params["startSector"] < 1):
                        logicalSectorNo -= 1
                    elif (params["startSector"] > 1):
                        logicalSectorNo += params["startSector"]-1

                    sectorMap[ofs]   = cyl & 0xFF
                    sectorMap[ofs+1] = cyl >> 8
                    sectorMap[ofs+2] = logicalSectorNo
                    sectorMap[ofs+3] = params["sdh"] | head
                    pos += 1
                    ofs += 4
                self._outputFile.write(sectorMap)
                
                # data records
                pos = 0
                ofs = 0
                while (pos < params["spt"]):
                    compressedData = True
                    prevData = data[ofs+0]
                    idx = 1
                    while (idx < params["ssize"]):
                        if (data[ofs+idx] != prevData):
                            compressedData = False
                            break
                        prevData = data[ofs+idx]
                        idx += 1
                        
                    if (compressedData):
                        self._outputFile.write(bytes([0x81]))
                        self._outputFile.write(bytes([prevData]))
                    else:
                        self._outputFile.write(bytes([1]))
                        self._outputFile.write(data[ofs:(ofs+params["ssize"])])
                        
                    ofs += params["ssize"]
                    pos += 1
            except:
                print("File write error")
                return cyl
            
            # advance
            head += 1
            if (head == params["heads"]):
                head = 0
                cyl += 1
            if (cyl == params["cylinders"]):
                return cyl
                
        return cyl        
            
    def getInterleaveTable(self, params, interleave):       
        table = bytearray([0]*(params["spt"]+1))
        pos = 1
        currSector = 1           
        while (currSector <= params["spt"]):
            table[pos] = currSector            
            currSector += 1
            pos += interleave
            if (pos > params["spt"]):
                pos = pos % params["spt"]
                while ((pos <= params["spt"]) and (table[pos]) > 0):
                    pos += 1
        return table        

if __name__ == "__main__":
    print("Not to be executed manually, use the scripts from one level up")
