# issues/184 (codegen): an attribute read through `cls` that does not fold
# to a constant. Before 2026-11-22 both backends refused it ("the attribute
# 'items' is not a field of the receiver's type 'B'"); FA gives each class
# its own contour, so the read is the class's prototype field.
import sys


class A:
    @classmethod
    def total(cls):
        return len(cls.items)

    @classmethod
    def first(cls):
        return cls.items[0]


class B(A):
    pass


class C(A):
    pass


B.items = [1] * (len(sys.argv) + 2)
C.items = ["x", "y"]
print(B.total(), C.total(), B.first(), C.first())
