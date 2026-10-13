# ifa/177: an `isinstance` guard on a MODULE-LEVEL variable must narrow the
# reads inside its branch, as it does for a function local. Each read of a
# module variable is a fresh load of its cell, and FA narrowed only the load
# the guard tested, so `x.f = 4` wrote an int into `f` on `{A, B}` -- into
# B's `f` too, which only ever holds a str.
class A:
    def __init__(self):
        self.f = 0


class B:
    def __init__(self):
        self.f = 'b'


xs = [A(), B()]
x = xs[0]
if isinstance(x, A):
    x.f = 4
    print(x.f + 1)
z = xs[1]
if isinstance(z, B):
    print(z.f.upper())
