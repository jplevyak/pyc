# struct: pack / unpack / unpack_from / calcsize for the INTEGER formats.
#
# Was a no-op stub (pack returned b"", unpack returned ()) -- issues/041.
# Its two blockers are gone: `bytes(list_of_ints)` works, and `*args` in a
# definition does (ROADMAP 6.1, pattern.cc's rest_wrapper).
#
# Supported: byte order `@ = < > !` (native is little-endian, as on every
# platform pyc targets), repeat counts, `x` padding, and the integer codes
# b B h H i I l L q Q. Anything else -- floats (e/f/d), `s`/`p` strings,
# `?` -- is not implemented and fails loudly rather than packing garbage.

def _size(c):
    if c == "b" or c == "B" or c == "x":
        return 1
    if c == "h" or c == "H":
        return 2
    if c == "i" or c == "I" or c == "l" or c == "L":
        return 4
    if c == "q" or c == "Q":
        return 8
    raise ValueError("struct: unsupported format character '" + c + "'")

def _signed(c):
    return c == "b" or c == "h" or c == "i" or c == "l" or c == "q"

def _little(fmt):
    # '@' and '=' are native order; pyc's targets are little-endian.
    if len(fmt) and fmt[0] == ">":
        return False
    if len(fmt) and fmt[0] == "!":
        return False
    return True

def _start(fmt):
    if len(fmt) and (fmt[0] == "<" or fmt[0] == ">" or fmt[0] == "!" or fmt[0] == "=" or fmt[0] == "@"):
        return 1
    return 0

def calcsize(fmt):
    n = 0
    count = 0
    i = _start(fmt)
    while i < len(fmt):
        c = fmt[i]
        i += 1
        if c >= "0" and c <= "9":
            count = count * 10 + (ord(c) - 48)
            continue
        if count == 0:
            count = 1
        n += count * _size(c)
        count = 0
    return n

def _byte(v):
    return __pyc_c_call__(bytes, "_CG_byte_from_int", int, v)

def pack(fmt, *args):
    # Built by concatenating one-byte bytes, not `bytes(list)`: the list's
    # conversion (list.__pyc_tobytes__) keeps internal str lists, and on the
    # start-merged list contour those merged with this int list, breaking
    # shedskin_examples/sha. Pack results are a few bytes long.
    little = _little(fmt)
    out = b""
    ai = 0
    count = 0
    i = _start(fmt)
    while i < len(fmt):
        c = fmt[i]
        i += 1
        if c >= "0" and c <= "9":
            count = count * 10 + (ord(c) - 48)
            continue
        if count == 0:
            count = 1
        size = _size(c)
        for _ in range(count):
            if c == "x":
                out = out + _byte(0)
                continue
            v = int(args[ai])
            ai += 1
            for k in range(size):
                if little:
                    shift = 8 * k
                else:
                    shift = 8 * (size - 1 - k)
                out = out + _byte(v >> shift)
        count = 0
    return out

def unpack_from(fmt, buffer, offset=0):
    little = _little(fmt)
    vals = []
    pos = offset
    count = 0
    i = _start(fmt)
    while i < len(fmt):
        c = fmt[i]
        i += 1
        if c >= "0" and c <= "9":
            count = count * 10 + (ord(c) - 48)
            continue
        if count == 0:
            count = 1
        size = _size(c)
        for _ in range(count):
            if c == "x":
                pos += 1
                continue
            v = 0
            for k in range(size):
                if little:
                    v = v | (buffer[pos + k] << (8 * k))
                else:
                    v = (v << 8) | buffer[pos + k]
            if _signed(c) and size < 8 and v >= (1 << (8 * size - 1)):
                v = v - (1 << (8 * size))
            vals.append(v)
            pos += size
        count = 0
    return tuple(vals)

# Field readers for the frontend's typed lowering of unpack/unpack_from
# (python_ifa_build_if1.cc, lower_struct_unpack). When the format is a
# constant string, `a, b, name = unpack_from('<II8s', data, off)` becomes
# the tuple literal
#   (_unpack_int(data, off, 0, 4, False, True),
#    _unpack_int(data, off, 4, 4, False, True),
#    _unpack_bytes(data, off, 8, 8))
# so each position gets its own type. The generic unpack_from below builds
# one list and can only ever return ints: a `s` field (bytes) next to an
# integer field would be a {int, bytes} element, which has no
# representation. shedskin rewrites struct.unpack the same way.

def _unpack_int(buffer, base, at, size, signed, little):
    pos = base + at
    v = 0
    for k in range(size):
        if little:
            v = v | (buffer[pos + k] << (8 * k))
        else:
            v = (v << 8) | buffer[pos + k]
    if signed and size < 8 and v >= (1 << (8 * size - 1)):
        v = v - (1 << (8 * size))
    return v

def _unpack_bytes(buffer, base, at, n):
    pos = base + at
    return bytes(buffer[pos:pos + n])

def unpack(fmt, buffer):
    return unpack_from(fmt, buffer, 0)
