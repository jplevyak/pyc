# issues/169: closure conversion of a function-local class. Its body and
# its methods read the enclosing function's locals; the class-body function
# receives them as extra formals and stores them on the prototype under
# hidden members, which instances copy and methods read through `self`.
# Before, both went through the lexical display: a method aborted the
# analysis (unique_AVar), and the class body, whose one contour served
# make2(2) and make2(3), silently read one call's value for both (`0 0`).
def inner(x):
    class I:
        def g(self):
            return x
    return I().g()
print(inner(1))
def make(x, y):
    class J:
        z = x
        def g(self, k):
            return self.z + y + k
        def h(self):
            f = lambda q: q * y
            return f(2)
    return J()
a = make(2, 10)
b = make(3, 20)
print(a.g(1), b.g(1), a.h(), b.h(), a.z, b.z)
def make2(x):
    class J:
        z = x
    j = J()
    return j.z
print(make2(2), make2(3))
