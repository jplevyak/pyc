# struct.pack's `*args` is a RECORD tuple; a bool beside ints, as in
# minpng's deflate_block, mixes numeric field types, so pack must not
# read it with a runtime index (tuple.__pyc_toints__ unrolls it).
import struct

def block(n, last=False):
    return struct.pack('<BHH', bool(last), n, 0xffff ^ n)

print(block(5))
print(block(7, last=True))
print(struct.pack('>IIBBBBB', 1600, 1200, 8, 2, 0, 0, 0))
print(struct.pack('<BH', True, 3))
