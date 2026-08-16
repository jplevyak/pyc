# issues/051: repr(bytes) must escape non-printable and non-ASCII bytes.
# CPython prints b'\x00\xff'; pyc emits the raw bytes instead, so the
# output is not valid repr and round-trips to something else.
#
# Found while pinning the semantics of bytes(list) for issues/050.
print(bytes([0, 255]))
print(bytes([9, 10, 13]))
