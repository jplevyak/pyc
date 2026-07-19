# issue 025 (minpng/plcfrs C-compile-error bucket, 2026-07-19): `str`
# had no __pyc_getslice__ of its own (unlike list/range/bytearray) --
# `s[i:j]` fell through to __pyc_any_type__'s generic
# self.__getitem__(slice(i,j,s)) fallback, and str.__getitem__
# unconditionally treats its key as a single int index. Even the
# simplest `"hello"[1:3]` produced a C compile error (a slice object
# passed where _CG_char_from_string expects an int) with a clean
# pyc compile otherwise -- string slicing had no test coverage
# before this fix (only single-char indexing, tests/string_index.py).
s = "hello world"
print(s[1:3])
print(s[6:])
print(s[:5])
print(s[:])
print(s[-5:])
print(s[:-6])
print(s[::2])
print(s[100:200])
print(s[3:1])
print(s[-100:100])
print(len(s[2:8]))
empty = ""
print(empty[1:3])

# real corpus shape: seq[i:i+n] in a loop (shedskin's minpng.py
# `pieces()`), the pattern that surfaced this bug originally.
def pieces(seq, n):
    return [seq[i:i + n] for i in range(0, len(seq), n)]

print(pieces("abcdefghij", 3))
