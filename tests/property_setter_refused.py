# issues/171 #13: a property setter needs a store dispatch pyc does not have;
# it is refused, never left unapplied (it compiled and then aborted).
class C:
    def __init__(self): self._x = 3
    @property
    def x(self): return self._x
    @x.setter
    def x(self, v): self._x = v + 1
c = C()
c.x = 5
print(c.x)
