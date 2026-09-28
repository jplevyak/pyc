# ifa/175: iterating a never-written list is reported as untyped when the
# loop's `position < len(...)` shares an int.__lt__ contour with an
# unrelated comparison on a non-constant int. CPython prints "neg" and 1.
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
