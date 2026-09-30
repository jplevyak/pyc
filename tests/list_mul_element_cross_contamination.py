# issues/039: `[None] * n`'s shared element representation let one
# heterogeneous list's element type leak into an unrelated, genuinely
# homogeneous one. `Cell.subp` really does hold Body|Cell|None; `Tree.bodies`
# only ever holds Body|None. Both lists come from the one `merge` creation
# point in `list.__mul__`.
#
# `t.bodies[i].tag()` is the witness: `tag` exists only on Body, so if Cell
# leaks into `bodies` again the call is refused. `subp` is read through
# `mass`, which both classes have. (The fixture used to call
# `c.subp[0].tag()` too. That is a Body-only method on a GENUINE union, so it
# is refused on its own, whatever happens to `bodies`. It masked the fact
# that 039 had been fixed. See issues/closed/039.)
class Node:
    def __init__(self):
        self.mass = 0.0

class Body(Node):
    def tag(self): return "Body"

class Cell(Node):
    def __init__(self):
        Node.__init__(self)
        self.subp = [None] * 2       # genuinely holds Body | Cell | None

class Tree:
    def __init__(self):
        self.bodies = [None] * 2     # only ever holds Body | None

    def build(self):
        c = Cell()
        c.subp[0] = Body()
        c.subp[1] = Cell()
        for i in range(2):
            self.bodies[i] = Body()
        return c

t = Tree()
c = t.build()
print(t.bodies[0].tag(), t.bodies[1].tag(), len(c.subp), c.subp[1].mass)
