# ifa/206, the interprocedural shape (softrender's Vertex.lerp ->
# Vector4.lerp -> sub). `P.t` holds an int-`z` V and a float-`z` V. In
# `V.lerp`, `dest.sub(self)` dispatches on DEST's method slot, so the `sub`
# clone an int-`z` dest installed is called with `r` = lerp's `self`, which
# may be either; it read `r.z` through the float layout. Before step 1 both
# backends printed 2.0 / 1.5 / 2.5 / 2.0 where CPython prints
# 0.5 / 1.5 / 2.5 / -1.5 (softrender's own ints are all 0, whose bits read
# as 0.0, so its output matched by luck). Step 1 refused it; step 2 gives
# the two clones of `V` their own class tags and tests the tag at the read,
# so it prints CPython's answer.
import sys
class V:
    def __init__(self, x, z):
        self.x, self.z = x, z
    def sub(self, r):
        return V(self.x - r.x - r.z, self.z - r.z)
    def mul(self, f):
        return V(self.x * f, self.z * f)
    def add(self, r):
        return V(self.x + r.x, self.z + r.z)
    def lerp(self, dest, a):
        return dest.sub(self).mul(a).add(self)
class P:
    def __init__(self, t):
        self.t = t
    def lerp(self, o, a):
        return P(self.t.lerp(o.t, a))
ps = [P(V(1.5, 3)), P(V(2.5, 7))]
for i in range(len(sys.argv)):
    ps.append(ps[-1].lerp(ps[-2], 0.5))
r = ps[0].lerp(ps[1], 0.5)
print(r.t.x)
for p in ps:
    print(p.t.x)
