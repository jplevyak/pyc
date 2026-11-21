# CPython's reflected binary operators: when the left operand's class has no
# __op__, the right operand's __rop__ is called (00_runtime.py).
class V:
    def __init__(self, s):
        self.s = s
    def __truediv__(self, o):
        return V(self.s + "/" + str(o))
    def __rtruediv__(self, o):
        return V(str(o) + "/" + self.s)
    def __radd__(self, o):
        return V(str(o) + "+" + self.s)
    def __rsub__(self, o):
        return V(str(o) + "-" + self.s)
    def __rmul__(self, o):
        return V(str(o) + "*" + self.s)
    def __rfloordiv__(self, o):
        return V(str(o) + "//" + self.s)
    def __rmod__(self, o):
        return V(str(o) + "%" + self.s)
    def __rpow__(self, o):
        return V(str(o) + "**" + self.s)
    def __rlshift__(self, o):
        return V(str(o) + "<<" + self.s)
    def __rrshift__(self, o):
        return V(str(o) + ">>" + self.s)
    def __rand__(self, o):
        return V(str(o) + "&" + self.s)
    def __ror__(self, o):
        return V(str(o) + "|" + self.s)
    def __rxor__(self, o):
        return V(str(o) + "^" + self.s)
    def __str__(self):
        return self.s

class W:
    def __str__(self):
        return "w"

v = V("v")
print(v / "a", "z" / v, "z" / (v / "b"))
print(W() + v, W() - v, W() * v, W() // v, W() ** v, W() % v)
print(W() << v, W() >> v, W() & v, W() | v, W() ^ v)
