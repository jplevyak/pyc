# issues/128 step 3: a MIXED field write -- a receiver whose classes split
# into those with their own writer of the field and those without -- is a
# DEMAND, asked on the converged types. The .env sets IFA_DBG_FIELDMIXED and
# the .check pins which writes raise it. Each union here is GENUINE (one slot
# holding both classes over time), so the rungs it feeds must decline, and the
# program must still print what CPython prints.

# 1. An attribute reassigned over time: `self.item` holds A1, then B1.
class A1:
    def __init__(self):
        self.f = 0
class B1:
    def __init__(self):
        self.g = 0
class Holder:
    def __init__(self):
        self.item = A1()
    def use(self):
        self.item.f = 2          # MIXED: the item slot is {A1, B1}
        return self.item.f
    def swap(self):
        self.item = B1()
        return self.item.g

# 2. A module-level variable reassigned over time.
class A2:
    def __init__(self):
        self.f = 0
class B2:
    def __init__(self):
        self.g = 0
cur = A2()
def set_b():
    global cur
    cur = B2()

# 3. A function local narrowed by isinstance: NOT a demand, the write lands
#    on A3 only.
class A3:
    def __init__(self):
        self.f = 0
class B3:
    def __init__(self):
        self.g = 0
def narrowed():
    xs = [A3(), B3()]
    for x in xs:
        if isinstance(x, A3):
            x.f = 4
    return xs[0].f + xs[1].g

h = Holder()
print(h.use())
print(h.swap())
cur.f = 1                        # MIXED: cur is {A2, B2}
print(cur.f)
set_b()
print(cur.g)
print(narrowed())
