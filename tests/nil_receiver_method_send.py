# A method send on a receiver that is one class or None -- here a list slot
# that starts None, as chull's `Face.vertex` does. In CPython `None == a`
# calls NoneType.__eq__.
#
# The classtag scan took `__pyc_None_type__.__eq__(self, x)` for a candidate
# on `x : V`, because `self` (None) has no `__eq__` of its own, so no null
# test was emitted and the call read a class tag through NULL (a segfault).
# And a null test decides such a call completely: CALLS reports it as
# `niltest`, not `dynamic`, and each arm is a direct call.
class V:
    def __init__(self, n):
        self.n = n
    def __str__(self):
        return "V%d" % self.n

class F:
    def __init__(self, vertex=[None, None]):
        self.vertex = []
        self.vertex.extend(vertex)

a = V(1)
f = F()
g = F([a, V(2)])
print(f.vertex[0] == a, g.vertex[0] == a, g.vertex[1] != a)
print("%s %s" % (f.vertex[0], g.vertex[1]))
f.vertex[0] = a
print(f.vertex[0] == a)
