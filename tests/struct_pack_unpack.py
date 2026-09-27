# struct.pack / unpack / calcsize for the integer formats (was a stub).
import struct
print(struct.pack('>I', 305419896), struct.pack('<I', 305419896))
print(struct.pack('>IIBBBBB', 1600, 1200, 8, 2, 0, 0, 0))
print(struct.pack('<BHH', 1, 5, 0xffff ^ 5))
print(struct.pack('<HH', 4660, 22136), struct.pack('>2xH', 1))
print(struct.calcsize('>IIBBBBB'), struct.calcsize('<HH4x'))
print(list(struct.unpack('>H', b'\x12\x34')), list(struct.unpack('<hB', b'\xff\xff\x07')))
