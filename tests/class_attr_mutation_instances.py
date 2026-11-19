# issues/173 item 1, instances: an instance (and a subclass) reads a class
# attribute through the class at READ time in CPython. pyc copies it into
# the instance when the instance is made, so a later store through the
# class is not seen. The class-counter idiom read through `self` is the
# common case.
class A:
    n = 2
a = A()
A.n = 9
print(a.n)

class B(A):
    pass
b = B()
A.n = 11
print(b.n, B.n)
a.n = 1
print(a.n, A.n)

class C:
    k = 0
    def bump(self):
        C.k += 1
        return self.k
c = C()
print(c.bump(), c.bump(), C.k)
