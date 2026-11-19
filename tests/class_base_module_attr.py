# issues/191: a base class named through a module, `class C(m.B)`. Modules
# are compile-time namespaces in pyc, so the base resolves as `m.B` does in
# an expression. Covers `import m`, `import m as a`, a package chain
# (`pyc_pkg.helper.Shape`), a module bound by `from . import helper` inside
# a package (pyc_pkg/shapes.py), and a base tuple.
import from_import_helper
import from_import_helper as fih
import pyc_pkg.helper
from pyc_pkg import shapes


class P3(from_import_helper.Point):
    def doubled(self):
        return super().doubled() + 1


class P4(fih.Point):
    pass


class Circle(pyc_pkg.helper.Shape):
    def __init__(self, r):
        super().__init__("circle")
        self.r = r

    def area(self):
        return 3 * self.r * self.r


class Tag:
    def tag(self):
        return "tagged"


class TaggedPoint(fih.Point, Tag):
    pass


print(P3(5).doubled(), P4(2).x)
print(Circle(2).describe())
print(shapes.Square(3).describe())
t = TaggedPoint(4)
print(t.doubled(), t.tag())
print(isinstance(P3(1), fih.Point), isinstance(Circle(1), pyc_pkg.helper.Shape))
