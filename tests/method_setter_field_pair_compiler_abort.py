# issues/047: pyc ABORTS -- cg.cc:389 assert(cg_get_string(n->lvals[0]))
# in write_c_prim -- on this 20-line program. CPython prints 7.
#
# Narrowed: the two setters must be METHODS; the same program with free
# functions (`def seta(v, x): v.a = x`) compiles fine. __slots__ is not
# required. Shares a discovery path with issues/046 and nothing else.
class C:
    def why(self): return 7

class V:
    def __init__(self):
        self.a = None
        self.b = None

class S:
    def __init__(self): self.v = V()
    def seta(self, x): self.v.a = x
    def setb(self, x): self.v.b = x
    def run(self):
        self.seta(C())
        self.setb("t")
        c = self.v.a
        if c: return c.why()
        return 0

print(S().run())
