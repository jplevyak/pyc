# ifa/188: `f` is a data field on A and a method on B, and one A and one B
# share a list. CPython prints `0 0 method`.
#
# EXPECTED: a compile ERROR (.check_fail + .check). `xs[0]` reads the list's
# element, which holds {A, B}, so `xs[0].f` is {int, B.f bound method}: a
# number and an object in one value, with no representation. It used to
# compile with no diagnostic and abort at run time in `print`'s `__str__`
# dispatch ("matching function not found"), and the LLVM backend failed
# module verification.
class A:
    def __init__(self):
        self.f = 0


class B:
    def __init__(self):
        self.g = 0

    def f(self):
        return 'method'


xs = [A(), B()]
b = B()
print(xs[0].f, xs[1].g, b.f())
