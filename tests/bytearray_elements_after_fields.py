# ifa/200 A6, two LLVM-backend defects in one bytearray:
#  - the elements were indexed from the object's first byte instead of
#    after its struct (C's trailing `v[0]`), so they overlaid the class's
#    own fields: `length` appeared as bytes 88-90, and a large bytearray
#    overran its allocation (softrender's framebuffer segfaulted);
#  - an element (a uint8) was SIGN-extended to int, so 200 read as -56.
import sys

n = 27648 + len(sys.argv) - 1
c = bytearray(n)
c[5] = 200
c[n - 1] = 255
print(len(c), c[5], c[-1], sum(c))
print([(k, c[k]) for k in range(len(c)) if c[k] != 0])
b = bytearray([188, 5, 255])
print(b[0], b[1], b[2], b[0] * 2, float(b[2]))
