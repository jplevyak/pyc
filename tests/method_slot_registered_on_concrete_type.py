# ifa/187: a method slot is registered against a CreationSet's CONCRETE type
# (cs->type), the struct the constructor actually fills. It was looked up in
# cs->sym, the unspecialized class, whose method members are never live, so
# the constructor of `V(0, 0, 0, 0)` stored no `add`. The C backend also
# stores methods into the prototype, which hid it; under -b, `have.add(n)`
# on an instance from that constructor called through NULL (softrender's
# Mesh normals).
class V:
    def __init__(self, x, y, z, w=1.0):
        self.x, self.y, self.z, self.w = x, y, z, w

    def add(self, r):
        return V(self.x + r.x, self.y + r.y, self.z + r.z, self.w + r.w)

    def sub(self, r):
        return V(self.x - r.x, self.y - r.y, self.z - r.z, self.w - r.w)

pts = [V(1.0, 2.0, 3.0, 1), V(1.0, 2.0, 3.0, 1), V(4.0, 5.0, 6.0, 1)]
acc = {}
for p in pts:
    n = p.sub(V(0.5, 0.5, 0.5, 0))
    key = (p.x, p.y, p.z)
    if key in acc:
        have = acc[key]
    else:
        have = V(0, 0, 0, 0)
    acc[key] = have.add(n)
for k in sorted(acc):
    v = acc[k]
    print(k, v.x, v.y, v.z, v.w)
