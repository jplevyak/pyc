# array: a list of the values plus its typecode. The byte conversions
# (tobytes/tofile, frombytes/fromfile) cover the integer typecodes, packed
# little-endian (native on every platform pyc targets, as in struct.py);
# the float codes f/d fail loudly rather than pack garbage.

def _itemsize(c):
    if c == "b" or c == "B":
        return 1
    if c == "h" or c == "H":
        return 2
    if c == "i" or c == "I" or c == "l" or c == "L":
        return 4
    if c == "q" or c == "Q":
        return 8
    raise ValueError("array: byte conversion of typecode '" + c + "' is not supported")

def _signed(c):
    return c == "b" or c == "h" or c == "i" or c == "l" or c == "q"

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

    def __delitem__(self, i):
        del self.data[i]

    # Slices forward (i, j, s) unchanged: the list's runtime normalizes the
    # omitted-bound sentinels. A slice is an array of the same typecode, as
    # in CPython -- this class had no slicing, so `header[:18]` fell through
    # to __getitem__ and came back an int (rdb).
    def __pyc_getslice__(self, i, j, s):
        r = array(self.typecode)
        r.data = self.data.__pyc_getslice__(i, j, s)
        return r

    def __pyc_delslice__(self, i, j, s):
        self.data.__pyc_delslice__(i, j, s)

    def tolist(self):
        return list(self.data)

    def fromlist(self, l):
        for x in l:
            self.data.append(x)

    def frombytes(self, b):
        n = _itemsize(self.typecode)
        if len(b) % n:
            raise ValueError("bytes length not a multiple of item size")
        if n == 1 and not _signed(self.typecode):
            for v in b:
                self.data.append(v)
            return
        top = 1 << (8 * n - 1)
        k = 0
        while k < len(b):
            v = 0
            for q in range(n):
                v = v | (b[k + q] << (8 * q))
            if _signed(self.typecode) and v >= top:
                v = v - 2 * top
            self.data.append(v)
            k += n

    def fromfile(self, f, n):
        # CPython inserts the complete items that were read, then raises
        # EOFError if there were fewer than n (rdb catches it).
        size = _itemsize(self.typecode)
        b = f.read(n * size)
        k = len(b) // size
        self.frombytes(b[0:k * size])
        if k < n:
            raise EOFError("read() didn't return enough bytes")
