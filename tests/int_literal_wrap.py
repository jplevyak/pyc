# issues/177: a literal past int64 that still fits 64 bits (othello2's
# 0xFFFFFFFFFFFFFFFF board mask). CPython keeps it exact; pyc's int is
# int64, so permissive mode wraps it to its two's-complement value and
# says so. The bit pattern survives & | ^ and masked shifts, which is what
# bitboard code uses; what does not survive is any reading of the value
# itself, so this prints what pyc computes, not what CPython does. The
# --strict half is tests/int_literal_wrap_strict.py.
FULL = 0xFFFFFFFFFFFFFFFF
HIGH = 0x8000000000000000
print(FULL & 0xFF, (FULL >> 8) & 0xFF, HIGH & FULL == HIGH)
print(FULL, HIGH)
