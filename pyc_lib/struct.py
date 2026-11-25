# struct: pack / unpack / unpack_from / calcsize (issues/214).
#
# Every format code is supported: x c b B ? h H i I l L q Q n N P e f d s p,
# the byte orders @ = < > !, repeat counts, and native ('@', the default)
# sizes and alignment. Native order is little-endian, as on every platform
# pyc targets, and native sizes are LP64's ('l', 'n', 'P' are 8 bytes).
#
# A call whose format is a CONSTANT string is lowered by the frontend
# (python_ifa_build_if1.cc, lower_struct_call) to one call per field of the
# readers and writers below, so each position has its own type:
#   unpack_from('<If8s', data, off)
#     -> (_unpack_int(data, b, 0, 4, False, True),
#         _unpack_float(data, b, 4, 4, True),
#         _unpack_bytes(data, b, 8, 8))      with b = _unpack_base(...)
#   pack('<If', n, x) -> _cat(_pack_int(n, "I", 4, False, True),
#                             _pack_float(x, 4, True))
# A tuple of an int, a float and a bytes has a type per position; one list
# built by a loop does not. shedskin rewrites struct.unpack the same way.
#
# The loop versions (pack, unpack_from) are what a format computed at run
# time reaches. Their values pass through one list, which holds one type,
# so they take the integer codes (and x, ?) only; any other code there
# raises NotImplementedError naming the constant-format form.
#
# Integers are pyc's 64-bit ints, so 'Q' cannot hold a value of 2**63 or
# more: it unpacks as a negative number (issues/214).


class error(Exception):
    pass


_SPACE = " \t\n\r\x0b\x0c"


def _mode(fmt):
    # (index of the first code, native sizes and alignment?, little-endian?)
    if len(fmt):
        c = fmt[0]
        if c == "<":
            return (1, False, True)
        if c == ">" or c == "!":
            return (1, False, False)
        if c == "=":
            return (1, False, True)
        if c == "@":
            return (1, True, True)
    return (0, True, True)


def _size(c, native):
    if c == "x" or c == "c" or c == "b" or c == "B" or c == "?" or c == "s" or c == "p":
        return 1
    if c == "h" or c == "H" or c == "e":
        return 2
    if c == "i" or c == "I" or c == "f":
        return 4
    if c == "l" or c == "L":
        if native:
            return 8
        return 4
    if c == "q" or c == "Q" or c == "d":
        return 8
    if (c == "n" or c == "N" or c == "P") and native:
        return 8
    raise error("bad char in struct format")


def _signed(c):
    return c == "b" or c == "h" or c == "i" or c == "l" or c == "q" or c == "n"


def _integer_code(c):
    return c != "x" and c != "c" and c != "?" and c != "s" and c != "p" and c != "e" and c != "f" and c != "d"


def _codes(fmt):
    # The format as parallel lists: code, repeat count, offset of its first
    # item. Native mode aligns each code to its size (no trailing padding,
    # as in CPython: '0q' at the end is how a program asks for it).
    start, native, little = _mode(fmt)
    codes = []
    counts = []
    offsets = []
    at = 0
    count = -1
    i = start
    while i < len(fmt):
        c = fmt[i]
        i += 1
        if c >= "0" and c <= "9":
            if count < 0:
                count = 0
            count = count * 10 + (ord(c) - 48)
            continue
        if c in _SPACE:
            if count >= 0:
                raise error("bad char in struct format")
            continue
        n = 1
        if count >= 0:
            n = count
        count = -1
        size = _size(c, native)
        if native and size > 1:
            at = (at + size - 1) // size * size
        codes.append(c)
        counts.append(n)
        offsets.append(at)
        if c == "s" or c == "p":
            at += n
        else:
            at += n * size
    if count >= 0:
        raise error("repeat count given without format specifier")
    return codes, counts, offsets, at


def calcsize(fmt):
    codes, counts, offsets, size = _codes(fmt)
    return size


def _items(codes, counts):
    n = 0
    for k in range(len(codes)):
        c = codes[k]
        if c == "s" or c == "p":
            n += 1
        elif c != "x":
            n += counts[k]
    return n


def _byte(v):
    return __pyc_c_call__(bytes, "_CG_byte_from_int", int, v)


def _pad(n):
    out = b""
    for _ in range(n):
        out = out + _byte(0)
    return out


def _cat(a, b):
    return a + b


def _uint_bytes(v, size, little):
    out = b""
    for k in range(size):
        if little:
            shift = 8 * k
        else:
            shift = 8 * (size - 1 - k)
        out = out + _byte(v >> shift)
    return out


def _range_error(c, lo, hi):
    return error("'" + c + "' format requires " + lo + " <= number <= " + hi)


def _check_int(v, c, size, signed):
    if size == 8:
        if not signed and v < 0:
            raise _range_error(c, "0", "18446744073709551615")
        return
    top = 1 << (8 * size)
    if signed:
        if v < -(top >> 1) or v >= (top >> 1):
            raise _range_error(c, str(-(top >> 1)), str((top >> 1) - 1))
    elif v < 0 or v >= top:
        raise _range_error(c, "0", str(top - 1))


def _pack_int(v, c, size, signed, little):
    if not isinstance(v, int):
        raise error("required argument is not an integer")
    n = v
    if isinstance(v, bool):  # its value as a plain int (not int(): see bool.__and__)
        n = 1 if v else 0
    _check_int(n, c, size, signed)
    return _uint_bytes(n, size, little)


def _pack_bool(v):
    if v:
        return _byte(1)
    return _byte(0)


def _float_bits(x, size):
    if size == 8:
        return __pyc_c_call__(int, "_CG_float_bits64", float, x)
    if size == 4:
        b = __pyc_c_call__(int, "_CG_float_bits32", float, x)
        if x - x == 0.0 and (b & 0x7fffffff) == 0x7f800000:
            raise OverflowError("float too large to pack with f format")
        return b
    b = __pyc_c_call__(int, "_CG_float_bits16", float, x)
    if x - x == 0.0 and (b & 0x7fff) == 0x7c00:
        raise OverflowError("float too large to pack with e format")
    return b


def _float_of_bits(b, size):
    if size == 8:
        return __pyc_c_call__(float, "_CG_float_from_bits64", int, b)
    if size == 4:
        return __pyc_c_call__(float, "_CG_float_from_bits32", int, b)
    return __pyc_c_call__(float, "_CG_float_from_bits16", int, b)


def _pack_float(v, size, little):
    if not isinstance(v, float) and not isinstance(v, int):
        raise error("required argument is not a float")
    return _uint_bytes(_float_bits(float(v), size), size, little)


def _is_bytes(v):
    return isinstance(v, bytes) or isinstance(v, bytearray)


def _pack_bytes(v, n):
    if not _is_bytes(v):
        raise error("argument for 's' must be a bytes object")
    d = bytes(v[0:n])
    return d + _pad(n - len(d))


def _pack_pascal(v, n):
    if not _is_bytes(v):
        raise error("argument for 'p' must be a bytes object")
    if n == 0:
        return b""
    k = len(v)
    if k > n - 1:
        k = n - 1
    if k > 255:
        k = 255
    d = bytes(v[0:n - 1])
    return _byte(k) + d + _pad(n - 1 - len(d))


def _pack_char(v):
    if not _is_bytes(v) or len(v) != 1:
        raise error("char format requires a bytes object of length 1")
    return bytes(v)


def _pack_count_error(expected, got):
    raise error("pack expected " + str(expected) + " items for packing (got " + str(got) + ")")
    return b""


def pack(fmt, *args):
    # A run-time format: integer codes only (see the header).
    # Built by concatenating one-byte bytes, not `bytes(list)`: the list's
    # conversion (list.__pyc_tobytes__) keeps internal str lists, and on the
    # start-merged list contour those merged with this int list, breaking
    # shedskin_examples/sha. Pack results are a few bytes long.
    start, native, little = _mode(fmt)
    codes, counts, offsets, size = _codes(fmt)
    n = _items(codes, counts)
    if n != len(args):
        _pack_count_error(n, len(args))
    # Converted per position (tuple.__pyc_toints__, inject_tuple_methods)
    # before any runtime index: `args[ai]` on a record whose fields mix
    # numeric types -- minpng's (bool, int, int) -- has no layout.
    vals = args.__pyc_toints__()
    out = b""
    ai = 0
    for k in range(len(codes)):
        c = codes[k]
        out = out + _pad(offsets[k] - len(out))
        if c == "x":
            out = out + _pad(counts[k])
            continue
        if c == "?":
            for _ in range(counts[k]):
                out = out + _pack_bool(vals[ai])
                ai += 1
            continue
        if not _integer_code(c):
            raise NotImplementedError("struct: the '" + c + "' code needs a constant format string in pyc")
        sz = _size(c, native)
        for _ in range(counts[k]):
            v = vals[ai]
            ai += 1
            _check_int(v, c, sz, _signed(c))
            out = out + _uint_bytes(v, sz, little)
    return out


def _unpack_base(buffer, offset, size, is_from):
    # The checks CPython makes before reading; returns the offset to read at.
    n = len(buffer)
    if not is_from:
        if n != size:
            raise error("unpack requires a buffer of " + str(size) + " bytes")
        return 0
    if offset < 0:
        if offset + n < 0:
            raise error("offset " + str(offset) + " out of range for " + str(n) + "-byte buffer")
        offset += n
    if n - offset < size:
        if offset > n:
            raise error("offset " + str(offset) + " out of range for " + str(n) + "-byte buffer")
        raise error("unpack_from requires a buffer of at least " + str(size + offset) + " bytes for unpacking " +
                    str(size) + " bytes at offset " + str(offset) + " (actual buffer size is " + str(n) + ")")
    return offset


def unpack_from(fmt, buffer, offset=0):
    # A run-time format: integer codes only (see the header).
    start, native, little = _mode(fmt)
    codes, counts, offsets, size = _codes(fmt)
    base = _unpack_base(buffer, offset, size, True)
    vals = []
    for k in range(len(codes)):
        c = codes[k]
        if c == "x":
            continue
        if not _integer_code(c):
            raise NotImplementedError("struct: the '" + c + "' code needs a constant format string in pyc")
        sz = _size(c, native)
        for q in range(counts[k]):
            vals.append(_unpack_int(buffer, base, offsets[k] + q * sz, sz, _signed(c), little))
    return tuple(vals)


def unpack(fmt, buffer):
    _unpack_base(buffer, 0, calcsize(fmt), False)
    return unpack_from(fmt, buffer, 0)


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


def _unpack_bool(buffer, base, at):
    return buffer[base + at] != 0


def _unpack_float(buffer, base, at, size, little):
    return _float_of_bits(_unpack_int(buffer, base, at, size, False, little), size)


def _unpack_bytes(buffer, base, at, n):
    pos = base + at
    return bytes(buffer[pos:pos + n])


def _unpack_pascal(buffer, base, at, n):
    if n == 0:
        return b""
    pos = base + at
    k = buffer[pos]
    if k >= n:
        k = n - 1
    return bytes(buffer[pos + 1:pos + 1 + k])
