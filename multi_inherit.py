import sys
class A:
    def __init__(self):
        self.a = int(sys.argv[1]) if len(sys.argv) > 1 else 1
    def f(self):
        return self.a
class B:
    def __init__(self):
        self.b = int(sys.argv[1]) + 1 if len(sys.argv) > 1 else 2
    def g(self):
        return self.b
class C(A, B):
    def __init__(self):
        A.__init__(self)
        B.__init__(self)
c = C()
print(c.f())
print(c.g())
