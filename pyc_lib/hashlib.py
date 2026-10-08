# pyc shim for `hashlib`.
#
# md5 is a real implementation (RFC 1321), so `hashlib.md5(data).hexdigest()`
# is CPython's answer (rsync, the only corpus user). The other algorithms
# raise NotImplementedError: issues/041's rule is "real or raise", and the
# stub that stood here returned "" from every digest, a silent wrong answer.

_MD5_S = [
    7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22,
    5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20,
    4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
    6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21,
]

_MD5_K = [
    3614090360, 3905402710, 606105819, 3250441966, 4118548399, 1200080426, 2821735955, 4249261313,
    1770035416, 2336552879, 4294925233, 2304563134, 1804603682, 4254626195, 2792965006, 1236535329,
    4129170786, 3225465664, 643717713, 3921069994, 3593408605, 38016083, 3634488961, 3889429448,
    568446438, 3275163606, 4107603335, 1163531501, 2850285829, 4243563512, 1735328473, 2368359562,
    4294588738, 2272392833, 1839030562, 4259657740, 2763975236, 1272893353, 4139469664, 3200236656,
    681279174, 3936430074, 3572445317, 76029189, 3654602809, 3873151461, 530742520, 3299628645,
    4096336452, 1126891415, 2878612391, 4237533241, 1700485571, 2399980690, 4293915773, 2240044497,
    1873313359, 4264355552, 2734768916, 1309151649, 4149444226, 3174756917, 718787259, 3951481745,
]


def _rotl32(x, c):
    x = x & 0xffffffff
    return ((x << c) | (x >> (32 - c))) & 0xffffffff


class _md5:
    def __init__(self, data):
        self._data = b""
        self.name = "md5"
        self.digest_size = 16
        self.block_size = 64
        self.update(data)

    def update(self, data):
        self._data = self._data + bytes(data)

    def digest(self):
        msg = self._data
        n = len(msg)
        pad = [0x80]
        while (n + len(pad)) % 64 != 56:
            pad.append(0)
        bits = (n * 8) & 0xffffffffffffffff
        for i in range(8):
            pad.append((bits >> (8 * i)) & 0xff)
        msg = msg + bytes(pad)
        a0 = 0x67452301
        b0 = 0xefcdab89
        c0 = 0x98badcfe
        d0 = 0x10325476
        m = [0] * 16
        for off in range(0, len(msg), 64):
            for j in range(16):
                p = off + 4 * j
                m[j] = msg[p] | (msg[p + 1] << 8) | (msg[p + 2] << 16) | (msg[p + 3] << 24)
            a = a0
            b = b0
            c = c0
            d = d0
            for i in range(64):
                if i < 16:
                    f = (b & c) | ((~b) & d)
                    g = i
                elif i < 32:
                    f = (d & b) | ((~d) & c)
                    g = (5 * i + 1) % 16
                elif i < 48:
                    f = b ^ c ^ d
                    g = (3 * i + 5) % 16
                else:
                    f = c ^ (b | ((~d) & 0xffffffff))
                    g = (7 * i) % 16
                f = (f + a + _MD5_K[i] + m[g]) & 0xffffffff
                a = d
                d = c
                c = b
                b = (b + _rotl32(f, _MD5_S[i])) & 0xffffffff
            a0 = (a0 + a) & 0xffffffff
            b0 = (b0 + b) & 0xffffffff
            c0 = (c0 + c) & 0xffffffff
            d0 = (d0 + d) & 0xffffffff
        out = []
        for w in [a0, b0, c0, d0]:
            for i in range(4):
                out.append((w >> (8 * i)) & 0xff)
        return bytes(out)

    def hexdigest(self):
        h = ""
        for x in self.digest():
            h += "%02x" % x
        return h

    def copy(self):
        r = _md5(b"")
        r._data = self._data
        return r


def md5(arg=b""):
    return _md5(arg)


def _not_implemented(name):
    raise NotImplementedError("pyc: hashlib." + name + " is not implemented (issues/041)")


def sha1(arg=b""):
    _not_implemented("sha1")


def sha224(arg=b""):
    _not_implemented("sha224")


def sha256(arg=b""):
    _not_implemented("sha256")


def sha384(arg=b""):
    _not_implemented("sha384")


def sha512(arg=b""):
    _not_implemented("sha512")
