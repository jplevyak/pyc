# issues/171 #13, issues/007: @property getters. A read of a property name is
# dispatched per receiver class: the getter where the class defines the
# property, the field where it has a data attribute of that name (D), and the
# builtin's own attribute where the name collides with one (list.count).
class C:
    def __init__(self): self._x = 3
    @property
    def x(self): return self._x * 2
class D:
    def __init__(self): self.x = 10
def show(o):
    return o.x + 1
print(C().x)
print(show(C()), show(D()))
objs = [C(), C()]
print(sum([o.x for o in objs]))
class E(C):
    pass
print(E().x)
class F:
    def __init__(self): self.items = [1, 2, 3]
    @property
    def count(self): return len(self.items)
print(F().count, [1, 1, 2].count(1))
