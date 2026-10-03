# struct.unpack / unpack_from with a constant format: every position has its
# own type, so an `s` (bytes) field can sit beside integer fields -- the
# shapes shedskin_examples/doom reads its WAD with ('<II8s', '<hh8s8sHhh',
# '<8sIHHIH'). The generic unpack_from builds one list and could only ever
# return ints, so `name` below had no type.
from struct import pack, unpack, unpack_from

data = b'\x02\x00\x00\x00\x10\x00\x00\x00E1M1\x00\x00\x00\x00\xff\xfe' + b'THINGS\x00\x00' + b'\x01\x02'
offset, length, name = unpack_from('<II8s', data, 0)
print(offset, length, name.rstrip(b'\0'))
a, b = unpack_from('<hh', data, 16)
print(a, b)
lump, = unpack_from('<8s', data, 18)
print(lump)
x, y = unpack_from('>BB', data, 26)
print(x, y)
pad, = unpack_from('<2xH', data, 24)
print(pad)
w, = unpack('<i', pack('<i', -123456))
print(w)
big, = unpack('>H', b'\x12\x34')
print(big)
print(b'flat\x00\x00'.rstrip(b'\0').upper(), b'x \n'.rstrip(), b'NUKAGE1'.startswith(b'NUKAGE'), b'AB'.startswith(b'ABC'))
print(b'F_SKY1' in b'F_SKY1\x00\x00', b'X' in b'abc', b'' in b'abc', b'F_SKY1'.replace(b'F_', b''))
