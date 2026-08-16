# issues/050: bytes(a_list) goes through __pyc__/04_sequence.py's
# list.__pyc_tobytes__, which builds the result with `r = r + chr(v)` per
# element and is therefore O(n^2) -- 25 s for 400 000 elements against
# CPython's 0.002 s.
#
# This test pins the SEMANTICS, not the speed, so that the fix (an
# allocate-once buffer helper) cannot change behaviour while making it
# linear: values are truncated to their low 8 bits rather than raising,
# and the empty list gives b"".
#
# Non-printable byte values are deliberately NOT exercised here: pyc's
# repr(bytes) does not escape them, which is a separate divergence pinned
# by tests/bytes_repr_escapes.py.
print(bytes([65, 66, 67]))
print(bytes([]))
print(len(bytes([7] * 300)))
