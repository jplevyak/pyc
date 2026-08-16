# issues/039: `[None] * n`'s shared element representation lets one
# heterogeneous list's element type leak into an unrelated, genuinely
# homogeneous one. `Cell.subp` really does hold Body|Cell|None; `Tree.bodies`
# only ever holds Body|None -- but pyc types the latter with Cell too and
# warns on `t.bodies[0].tag()`. The program still runs correctly, so this
# pins the precision loss, which is 039's actual subject.
#
# Check files describe the CORRECT behaviour (no diagnostics, "Body Body");
# the .known_issue tag keeps it out of the failure count.
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
print(t.bodies[0].tag(), c.subp[0].tag())
