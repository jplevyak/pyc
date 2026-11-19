# ifa/200 A5: an int moved into a slot FA widened to float64 must be
# CONVERTED, not stored as raw bits. The LLVM backend stored the i64's bits
# into the double slot, so `5` read back as 2.5e-323 (the C backend casts).
# The condition is runtime-varying so FA cannot fold a branch away; at run
# time it is true, which takes the int branch. The values are printed
# through `==` and `+ 0.5` so the output is CPython's: the slot itself
# prints 5.0 for CPython's 5 (permissive numeric coercion, issues/171,
# warned), and stored raw bits would print False and 0.5.
import sys

def local_slot(c):
    if c:
        x = 5
    else:
        x = 2.5
    return x

if len(sys.argv) < 5:
    g = 7
else:
    g = 0.5

x = local_slot(len(sys.argv) < 5)
print(x == 5, x + 0.5)
print(g == 7, g + 0.5)
print(x + g == 12)
