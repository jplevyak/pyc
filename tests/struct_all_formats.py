# struct: every format code, byte order, native alignment and CPython's
# errors, with constant formats (each field typed on its own, issues/214).
import struct
from struct import pack, unpack, unpack_from, calcsize

# floats: e f d, both byte orders
print(pack('<d', 1.5), pack('>d', -0.1), pack('<f', 0.1), pack('>e', 65504.0))
print(unpack('<d', pack('<d', 3.141592653589793)), unpack('>f', pack('>f', 0.1)))
print(unpack('<e', pack('<e', 0.333)), unpack('<e', b'\x00\x7c'), unpack('<f', b'\x00\x00\xc0\x7f'))
print(pack('<f', float('-inf')), pack('<d', 2), pack('<f', True), pack('<e', 1e-8))

# mixed records: int, float, bytes, bool, char, pascal
rec = pack('<If8s?c', 7, 2.5, b'name', True, b'Z')
print(rec, len(rec))
n, x, name, flag, ch = unpack('<If8s?c', rec)
print(n, x, name.rstrip(b'\x00'), flag, ch)
print(pack('<5p', b'abcdefg'), pack('<5p', b'ab'), unpack('<5p', b'\x02abzz'), pack('<3s', b'abcdef'))
print(unpack('<??', b'\x00\x05'), pack('<??', 0, 'x'), pack('<0s', b'abc'), unpack('<0s', b''))

# member form and native alignment and sizes
print(struct.pack('bi', 1, 2), struct.pack('@bq', 1, 2), struct.pack('=bq', 1, 2))
print(calcsize('bi'), calcsize('@l'), calcsize('<l'), calcsize('@i0q'), calcsize('ci'), calcsize('<5p'), calcsize('n'))
print(struct.unpack('@hd', struct.pack('@hd', -3, 0.25)), struct.unpack('nNP', struct.pack('nNP', -1, 2, 3)))
print(pack('< I H', 1, 2), pack('!h', -2), unpack_from('>Hd', b'\xff' + pack('>Hd', 513, 1e100), 1))
print(unpack_from('<h', b'\x01\x02\x03', -2))

def err(f):
    try:
        f()
    except struct.error as e:
        print('error:', e)
    except OverflowError as e:
        print('OverflowError:', e)

err(lambda: pack('<h', 70000))
err(lambda: pack('<B', 300))
err(lambda: pack('B', -1))
err(lambda: pack('<b', 200))
err(lambda: pack('<L', -1))
err(lambda: pack('<Q', -1))
err(lambda: pack('<i', 1.0))
err(lambda: pack('<f', 'x'))
err(lambda: pack('<f', 1e40))
err(lambda: pack('<e', 1e6))
err(lambda: pack('<s', 'a'))
err(lambda: pack('<c', b'ab'))
err(lambda: pack('<I', 1, 2))
err(lambda: unpack('<I', b'ab'))
err(lambda: unpack_from('<I', b'ab', 1))
err(lambda: unpack_from('<I', b'ab', -5))
err(lambda: pack('<n', 1))
err(lambda: calcsize('<2 I'))
err(lambda: calcsize('<3'))
err(lambda: calcsize('<z'))
