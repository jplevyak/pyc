class A:
    def f(self):
        return 1
class B:
    def g(self):
        return 2
class C(A, B):
    pass
c = C()
print(c.f())
print(c.g())
