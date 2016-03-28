import sys
import struct

lines = open(sys.argv[1]).read().split('\n')
output = open(sys.argv[2], 'w')

output.write("SIMv0.0 ")
# 0 attributes 0 uniforms
output.write(struct.pack("2B", 0, 0))

bc = []
for line in lines:
    parts = line.lstrip().rstrip().split(' ')
    if parts[0] == 'add':
        bc.append(0)
        bc.append(int(parts[1]))
        bc.append(int(parts[2]))
        bc.append(int(parts[3]))
    elif parts[0] == 'sub':
        bc.append(1)
        bc.append(int(parts[1]))
        bc.append(int(parts[2]))
        bc.append(int(parts[3]))
    elif parts[0] == 'mul':
        bc.append(2)
        bc.append(int(parts[1]))
        bc.append(int(parts[2]))
        bc.append(int(parts[3]))
    elif parts[0] == 'div':
        bc.append(3)
        bc.append(int(parts[1]))
        bc.append(int(parts[2]))
        bc.append(int(parts[3]))
    elif parts[0] == 'pow':
        bc.append(4)
        bc.append(int(parts[1]))
        bc.append(int(parts[2]))
        bc.append(int(parts[3]))
    elif parts[0] == 'movf':
        bc.append(5)
        bc.append(int(parts[1]))
        bc += [ord(c) for c in struct.pack('<f', float(parts[2]))]
    elif parts[0] == 'sqrt':
        bc.append(6)
        bc.append(int(parts[1]))
        bc.append(int(parts[2]))
    elif parts[0] == 'less':
        bc.append(8)
        bc.append(int(parts[1]))
        bc.append(int(parts[2]))
        bc.append(int(parts[3]))
    elif parts[0] == 'greater':
        bc.append(9)
        bc.append(int(parts[1]))
        bc.append(int(parts[2]))
        bc.append(int(parts[3]))
    elif parts[0] == 'equal':
        bc.append(10)
        bc.append(int(parts[1]))
        bc.append(int(parts[2]))
        bc.append(int(parts[3]))
    elif parts[0] == 'and':
        bc.append(11)
        bc.append(int(parts[1]))
        bc.append(int(parts[2]))
        bc.append(int(parts[3]))
    elif parts[0] == 'or':
        bc.append(12)
        bc.append(int(parts[1]))
        bc.append(int(parts[2]))
        bc.append(int(parts[3]))
    elif parts[0] == 'not':
        bc.append(13)
        bc.append(int(parts[1]))
        bc.append(int(parts[2]))
    elif parts[0] == 'sel':
        bc.append(14)
        bc.append(int(parts[1]))
        bc.append(int(parts[2]))
        bc.append(int(parts[3]))
        bc.append(int(parts[4]))
    elif parts[0] == 'beginif':
        bc.append(15)
        bc.append(int(parts[1]))
        bc += [ord(c) for c in struct.pack('<L', int(parts[2]))]
        bc.append(int(parts[3]))
        bc.append(int(parts[4]))
    elif parts[0] == 'endif':
        bc.append(16)
    elif parts[0] == 'beginwhile':
        bc.append(17)
        bc.append(int(parts[1]))
        bc += [ord(c) for c in struct.pack('<L', int(parts[2]))]
        bc.append(int(parts[3]))
        bc.append(int(parts[4]))
        bc += [ord(c) for c in struct.pack('<L', int(parts[5]))]
        bc.append(int(parts[6]))
        bc.append(int(parts[7]))
    elif parts[0] == 'endwhilecond':
        bc.append(18)
    elif parts[0] == 'endwhile':
        bc.append(19)
    elif parts[0] == 'rand':
        bc.append(22)
        bc.append(int(parts[1]))
    elif parts[0] == 'floor':
        bc.append(23)
        bc.append(int(parts[1]))
        bc.append(int(parts[2]))
    elif parts[0] == 'mov':
        bc.append(24)
        bc.append(int(parts[1]))
        bc.append(int(parts[2]))

output.write(struct.pack("<L", len(bc)))
output.write(''.join([chr(c) for c in bc]))
