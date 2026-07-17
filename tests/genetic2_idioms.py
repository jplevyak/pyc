# The genetic2 dig's fixed idioms (pyc issues/025, 2026-07-16), each
# a standalone micro that failed before its fix:
#  1. max/min with key= (was: no formal to bind key -> untyped cascade)
#  2. a method named like an imported module (was: class scope leaked
#     into method bodies; `copy` resolved to the METHOD)
#  3. dynamic tuple(list) / list(tuple) (fixed-arity tuples -> list)
#  4. Optional[list] field guards and container ops (bool-context
#     and/or lowering; nil getslice stub; list+tuple concat)
#  5. constant-format % with %s scalars and objects (was: raw C
#     varargs strlen'd an int64)
#  6. dynamic tuple + tuple concatenation (named values, not
#     literals -> list result)
import copy

class Item:
    def __init__(self, w):
        self.w = w
        self.parts = None
    def copy(self):
        # `copy` here is the MODULE (class scope must not shadow it
        # inside methods), and this method shares its name.
        return Item(copy.deepcopy(self.w))

def weight(it):
    return it.w

items = [Item(3.0), Item(1.0), Item(2.0)]
best = max(items, key=weight)
worst = min(items, key=weight)
print(best.w)
print(worst.w)

dup = items[0].copy()
dup.w = 9.5
print(items[0].w)
print(dup.w)

# Optional[list] field: guard, slice, concat with a 1-tuple, index.
root = Item(0.0)
root.parts = tuple([Item(7.0), Item(8.0)])
if root.parts:
    grafted = Item(6.0)
    root.parts = root.parts[:0] + (grafted,) + root.parts[1:]
    print(root.parts[0].w)
    print(root.parts[1].w)
print(len(root.parts))

# %-formatting: %s with int, float, and an object; %d/%f raw.
epoch = 9
fit = 0.75
print("Epoch: %s, best fitness: %s" % (epoch, fit))
print("n=%d f=%.2f s=%s" % (42, 1.5, "hi"))

class Tag:
    def __str__(self):
        return "tag!"
print("obj: %s" % Tag())

# Dynamic tuple concatenation of NAMED tuples (list result).
a_lines = (11, 12)
b_lines = (13, 14)
all_lines = a_lines + b_lines
for v in all_lines:
    print(v)
