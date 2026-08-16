# ifa/issues/074: the SETTER splitter (stage 3) -- data splitting on
# AssignSets, i.e. "which AVars wrote me".
#
# `put` writes its argument through to a field of whichever Box it is
# given. Two Boxes end up holding different classes, so separating them
# needs the WRITER (put's `b`), not just the type at a formal -- which is
# what the setter stage is for. Stage 3 only runs on a pass where stages
# 1-2 are quiet, so this also demonstrates the cascade reaching past
# stage 1.
#
# Both payloads are objects with a common method on purpose: an int/str
# pair would drag in the heterogeneous-print gap and the test would be
# pinning that instead of the splitter.
class A:
    def tag(self):
        return "A"

class B:
    def tag(self):
        return "B"

class Box:
    def __init__(self):
        self.v = None

def put(b, x):
    b.v = x

a = Box()
c = Box()
put(a, A())
put(c, B())
print(a.v.tag(), c.v.tag())
