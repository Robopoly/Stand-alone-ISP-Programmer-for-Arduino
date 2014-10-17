import sys

# python srcipt to merge 2 nibbles into a byte

if len(sys.argv) < 2:
    print "No file provided"
    exit()

output = open("output.txt", 'w')

print "Opening source file"
f = open(sys.argv[1], 'r')

def hex2nib(h):
    if h >= '0' and h <= '9':
        return int(h) - int('0')
    if h >= 'A' and h <= 'F':
        return (h - 'A') + 10

pairOdd = False
newChar = 0
newLine = ""
byteCount = 0

print "Processing"
for line in f:
    for char in line.strip():
        # line delimiter
        if char == ':':
            # new type will start with semicolon
            output.write(newLine)
            
            newLine = ""
            pairOdd = False
            continue
        
        # in case of odd character index merge with the one before
        if pairOdd:
            # add a new line character every 8 bytes after the first line
            if not byteCount % 8 and byteCount:
                newLine += "\n"
            
            # merge the 2 nibbles to a byte and output in binary
            newLine += format((newChar << 4) + int(char, 16), '#010b') + ", "
            newChar = 0
            
            byteCount += 1
        else:
            newChar = int(char, 16)
        
        pairOdd = not pairOdd

# last line (remove comma and space)
output.write(newLine[:-2])

print "Byte count: " + str(byteCount)

print "Output in output.txt file"
output.close()
f.close()
