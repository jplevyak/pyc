# ifa/102, softrender's shape: `components or bytearray(n)` with the
# literal b''. len() of a str/bytes CONSTANT is now a constant, so its
# truth folds and the `or` yields only the bytearray. Without the fold the
# value was {bytes, bytearray}, which share no run-time discriminator, and
# the C backend emitted a `matching function not found` stub that aborted
# on the first read.
#
# The fold needs the truth test's contours to see ONE constant. Adding
# `len("abc")` here breaks it: `str.__pyc_to_bool__` calls
# `self.__len__()`, so one `str.__len__` contour receives "" and "abc".
# CONST_DEMAND would have to split it, and its walk cannot get from the
# violation to that formal through `self.__len__() != 0`, a primitive whose
# result has no backward link to its operands (ifa/102).
class Bitmap:
    def __init__(self, n, components=None):
        self.components = components or bytearray(n)

def main():
    t = Bitmap(4, b'')
    r = Bitmap(2)
    print(t.components[0], len(t.components), r.components[1])
    y = "" or 5
    print(y + 1)

main()
