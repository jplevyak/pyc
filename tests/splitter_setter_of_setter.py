# ifa/issues/074: the SETTER_OF_SETTER splitter (stage 3b) -- setter
# splitting propagated one level further back through the write graph.
#
# One more indirection than tests/splitter_setter.py: `fill` writes through
# `h.b`, which was itself written by `hold`. Separating p from q therefore
# needs the writer OF the writer, which is what this stage is for. Stage 3b
# only runs when stage 3 itself has gone quiet.
class A:
    def tag(self):
        return "A"

class B:
    def tag(self):
        return "B"

class Box:
    def __init__(self):
        self.v = None

class Holder:
    def __init__(self):
        self.b = None

def put(box, x):
    box.v = x

def hold(h, box):
    h.b = box

def fill(h, x):
    put(h.b, x)

p = Holder()
q = Holder()
hold(p, Box())
hold(q, Box())
fill(p, A())
fill(q, B())
print(p.b.v.tag(), q.b.v.tag())
