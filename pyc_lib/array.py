# array: a list of the values plus its typecode (issues/214).
#
# The integer typecodes b B h H i I l L q Q are `array`; the float ones f d
# are `_floatarray`, a subclass whose list holds floats. CPython's array
# keeps one C buffer whatever the typecode; here the two element types need
# two classes, and which one a call builds is known only from its
# typecode, so the frontend picks it: `array('d', ...)` with a CONSTANT
# 'f' or 'd' is built as `_floatarray('d', ...)` (python_ifa_build_if1.cc,
# float_array_class). A float typecode not written as a constant reaches
# `array` and is refused at run time (NotImplementedError).
#
# Bytes are little-endian (native on every platform pyc targets, as in
# struct.py), at LP64's sizes ('l' is 8 bytes). 'u' is not provided.

def _itemsize(c):
    if c == "b" or c == "B":
        return 1
    if c == "h" or c == "H":
        return 2
    if c == "i" or c == "I" or c == "f":
        return 4
    if c == "l" or c == "L" or c == "q" or c == "Q" or c == "d":
        return 8
    raise ValueError("bad typecode (must be b, B, u, h, H, i, I, l, L, q, Q, f or d)")

def _signed(c):
    return c == "b" or c == "h" or c == "i" or c == "l" or c == "q"

def _repr(a):
    if len(a.data) == 0:
        return "array('" + a.typecode + "')"
    return "array('" + a.typecode + "', " + str(a.data) + ")"

class array:
    def __init__(self, typecode, initializer=None):
        if typecode == "f" or typecode == "d":
            raise NotImplementedError("array: typecode '" + typecode + "' must be a constant string in pyc")
        self.itemsize = _itemsize(typecode)
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

    def __contains__(self, x):
        return x in self.data

    def __repr__(self):
        return _repr(self)

    def __str__(self):
        return _repr(self)

    def tobytes(self):
        # bytes() of an int list keeps the low 8 bits of each value, which
        # is exactly the two's-complement byte a signed code packs.
        n = self.itemsize
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
        n = self.itemsize
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
            if _signed(self.typecode) and n < 8 and v >= top:
                v = v - 2 * top
            self.data.append(v)
            k += n

    def fromfile(self, f, n):
        # CPython inserts the complete items that were read, then raises
        # EOFError if there were fewer than n (rdb catches it).
        size = self.itemsize
        b = f.read(n * size)
        k = len(b) // size
        self.frombytes(b[0:k * size])
        if k < n:
            raise EOFError("read() didn't return enough bytes")


def _bits(x, n):
    if n == 4:
        return __pyc_c_call__(int, "_CG_float_bits32", float, x)
    return __pyc_c_call__(int, "_CG_float_bits64", float, x)

def _of_bits(b, n):
    if n == 4:
        return __pyc_c_call__(float, "_CG_float_from_bits32", int, b)
    return __pyc_c_call__(float, "_CG_float_from_bits64", int, b)


def _not_real(x):
    # CPython names the type; pyc has no type(x).__name__ (issues/190), so
    # the builtin types are named here and any other gets the bare message.
    if isinstance(x, str):
        return "must be real number, not str"
    if isinstance(x, bytes):
        return "must be real number, not bytes"
    if isinstance(x, list):
        return "must be real number, not list"
    if isinstance(x, tuple):
        return "must be real number, not tuple"
    if x is None:
        return "must be real number, not NoneType"
    return "must be real number"


class _floatarray(array):
    # 'f' stores each value rounded to binary32, as CPython's C float does,
    # so a read gives back what tobytes writes.
    def __init__(self, typecode, initializer=None):
        self.itemsize = _itemsize(typecode)
        self.typecode = typecode
        self.data = []
        if initializer:
            for x in initializer:
                self.data.append(self._fit(x))

    def _fit(self, x):
        if not isinstance(x, float) and not isinstance(x, int):
            raise TypeError(_not_real(x))
        v = float(x)
        if self.itemsize == 4:
            return _of_bits(_bits(v, 4), 4)
        return v

    def append(self, x):
        self.data.append(self._fit(x))

    def extend(self, iterable):
        for x in iterable:
            self.data.append(self._fit(x))

    def fromlist(self, l):
        for x in l:
            self.data.append(self._fit(x))

    def __setitem__(self, i, v):
        self.data[i] = self._fit(v)

    def __pyc_getslice__(self, i, j, s):
        r = _floatarray(self.typecode)
        r.data = self.data.__pyc_getslice__(i, j, s)
        return r

    def tobytes(self):
        n = self.itemsize
        out = []
        for v in self.data:
            b = _bits(v, n)
            for k in range(n):
                out.append((b >> (8 * k)) & 255)
        return bytes(out)

    def frombytes(self, b):
        n = self.itemsize
        if len(b) % n:
            raise ValueError("bytes length not a multiple of item size")
        k = 0
        while k < len(b):
            v = 0
            for q in range(n):
                v = v | (b[k + q] << (8 * q))
            self.data.append(_of_bits(v, n))
            k += n
