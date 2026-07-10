# issue 027: explicit unbound base-class method calls with an explicit
# receiver -- Base.method(self, ...) -- the standard idiom for invoking
# a SPECIFIC base's implementation (especially each base's __init__
# under multiple inheritance). Dispatch must be static (as the named
# class) while the receiver keeps its concrete type.
class A:
    def __init__(self):
        self.x = 1
    def get(self):
        return self.x

class C(A):
    def __init__(self):
        A.__init__(self)
        self.z = 3

def main():
    c = C()
    print(c.x)
    print(c.z)
    a = A()
    A.__init__(a)
    print(a.x)
    print(A.get(c))
main()
