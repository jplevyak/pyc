# issues/110: copy.deepcopy of a tuple deep-copies its elements, as in
# CPython. The any-type fallback it used to reach was a SHALLOW copy, so
# the copy shared `T` with the original and the write below showed
# through: pyc printed `5 5` where CPython prints `-1 5`. tuple now has an
# element-recursive __deepcopy__ (generated per arity, like __str__, so a
# heterogeneous tuple keeps each field's own type).
import copy

class T:
    def __init__(self):
        self.value = -1

t = (T(), "a", 3)
c = copy.deepcopy(t)
c[0].value = 5
print(t[0].value, c[0].value, c[1], c[2])

nested = ((T(), 1), [T()])
d = copy.deepcopy(nested)
d[0][0].value = 7
d[1][0].value = 8
print(nested[0][0].value, nested[1][0].value, d[0][0].value, d[1][0].value, d[0][1])
