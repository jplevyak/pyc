# array: a list of the values plus its typecode. tobytes/tofile cover the
# integer typecodes, packed little-endian (native on every platform pyc
# targets, as in struct.py); the float codes f/d fail loudly rather than
# pack garbage.

def _itemsize(c):
    if c == "b" or c == "B":
        return 1
    if c == "h" or c == "H":
        return 2
    if c == "i" or c == "I" or c == "l" or c == "L":
        return 4
    if c == "q" or c == "Q":
        return 8
    raise ValueError("array: tobytes of typecode '" + c + "' is not supported")

class array:
    def __init__(self, typecode, initializer=None):
        self.typecode = typecode
        self.data = list(initializer) if initializer else []
        
    def append(self, x):
        self.data.append(x)
        
    def extend(self, iterable):
        for x in iterable:
            self.data.append(x)
            
    def __len__(self):
        return len(self.data)
        
    def __getitem__(self, i):
        return self.data[i]
        
    def __setitem__(self, i, v):
        self.data[i] = v
        
    def __iter__(self):
        return iter(self.data)

    def tobytes(self):
        # bytes() of an int list keeps the low 8 bits of each value, which
        # is exactly the two's-complement byte a signed code packs.
        n = _itemsize(self.typecode)
        if n == 1:
            return bytes(self.data)
        out = []
        for v in self.data:
            for k in range(n):
                out.append((v >> (8 * k)) & 255)
        return bytes(out)

    def tofile(self, f):
        # CPython's tofile is f.write(self.tobytes()) for a binary file.
        f.write(self.tobytes())
