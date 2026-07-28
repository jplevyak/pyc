# `x == None` / `x != None` used to dispatch through the generic
# __eq__/__ne__ method (map_pyop_to_cmp), unlike `x is None` /
# `x is not None`, which the frontend already lowers straight to
# an isinstance-against-nil check.  For a container-typed `x` that
# crashes at runtime: list.__eq__'s body assumes its argument is
# ANOTHER list (`len(l)`, `l[i]`) and blows up when `l` is the None
# literal (shedskin_examples/chaos.py's
# `Spline.__init__(self, points, degree=3, knots=None): if knots
# == None: ...`).  `==`/`!=` against a None literal now get the
# same isinstance-based lowering as `is`/`is not`.


def describe(knots):
    if knots == None:
        return "none"
    else:
        return "list:" + str(len(knots))


print(describe(None))
print(describe([1, 2, 3]))

a = [1, 2, 3]
print(a == None)
print(a != None)
print(None == a)
print(None != a)

b = None
print(b == None)
print(b != None)
print(None == None)
print(None != None)


class C:
    def __init__(self, x):
        self.x = x


c = C(5)
print(c == None)
print(c != None)
