# ifa/175 (fixed): iterating a never-written list must not be live just
# because the loop test `position < len(...)` shares an int.__lt__ contour
# with an unrelated non-constant comparison. CPython prints "neg" and 1.
import sys
def check(a):
    if a < 0:
        print("neg")
def empty():
    r = []
    return r
check(len(sys.argv) - 5)
L = [1]
L.extend(empty())
print(len(L))
