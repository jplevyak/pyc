# issues/177: under --strict, a literal past int64 is refused, because
# wrapping it changes what the program computes. The permissive half is
# tests/int_literal_wrap.py.
FULL = 0xFFFFFFFFFFFFFFFF
print(FULL & 0xFF)
