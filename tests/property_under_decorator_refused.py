# issues/007: a @property UNDER another decorator hands that decorator a
# property object, which pyc has no value for. Refused, never ignored.
def deco(p):
    return p
class C:
    def __init__(self): self._x = 3
    @deco
    @property
    def x(self): return self._x
print(C().x)
