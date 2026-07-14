# pyc shim for the standard `io` module: BytesIO/StringIO backed by a
# plain str buffer. pyc has no real `bytes` type distinct from `str`
# (see __pyc__/06_bytearray.py) and no b"..." literal grammar support
# (python.g's stringprefix is r/R/u/U only), so this operates on str
# throughout rather than true bytes -- close enough for build-a-buffer
# / parse-a-buffer use, not a byte-for-byte match of CPython's io.
# str has no working slice path yet (__pyc__/01_str.py), so reads are
# built via single-char indexing, same as pyc_lib/os.py and re.py.

class BytesIO:
    def __init__(self, initial=""):
        self.buf = initial
        self.pos = 0

    def read(self, n=-1):
        blen = len(self.buf)
        end = blen
        if n >= 0 and self.pos + n < blen:
            end = self.pos + n
        r = ""
        k = self.pos
        while k < end:
            r = r + self.buf[k]
            k += 1
        self.pos = end
        return r

    def readline(self):
        blen = len(self.buf)
        start = self.pos
        k = start
        while k < blen and self.buf[k] != '\n':
            k += 1
        if k < blen:
            k += 1
        r = ""
        i = start
        while i < k:
            r = r + self.buf[i]
            i += 1
        self.pos = k
        return r

    def tell(self):
        return self.pos

    def seek(self, p):
        self.pos = p
        return self.pos

    def write(self, s):
        self.buf = self.buf + s
        return len(s)

    def getvalue(self):
        return self.buf

    def close(self):
        pass

class StringIO:
    def __init__(self, initial=""):
        self.buf = initial
        self.pos = 0

    def read(self, n=-1):
        blen = len(self.buf)
        end = blen
        if n >= 0 and self.pos + n < blen:
            end = self.pos + n
        r = ""
        k = self.pos
        while k < end:
            r = r + self.buf[k]
            k += 1
        self.pos = end
        return r

    def readline(self):
        blen = len(self.buf)
        start = self.pos
        k = start
        while k < blen and self.buf[k] != '\n':
            k += 1
        if k < blen:
            k += 1
        r = ""
        i = start
        while i < k:
            r = r + self.buf[i]
            i += 1
        self.pos = k
        return r

    def tell(self):
        return self.pos

    def seek(self, p):
        self.pos = p
        return self.pos

    def write(self, s):
        self.buf = self.buf + s
        return len(s)

    def getvalue(self):
        return self.buf

    def close(self):
        pass
