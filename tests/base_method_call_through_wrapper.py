# issues/198: a class-qualified or super() call of a base method whose subclass
# overrides it must call the BASE method, whatever wrapper the call
# needs: a default, keywords out of order, *args, or f(*t).


class B:
    def __init__(self, level=0):
        self.level = level

    def dflt(self, a, b=10):
        return "B.dflt %d %d" % (a, b)

    def order(self, a, b):
        return "B.order %d %d" % (a, b)

    def star(self, *args):
        return "B.star %d" % len(args)

    def unpack(self, a, b):
        return "B.unpack %d %d" % (a, b)


class C(B):
    def __init__(self, s=1):
        self.s = s
        B.__init__(self)

    def dflt(self, a, b=20):
        return "C.dflt %d %d / " % (a, b) + B.dflt(self, a)

    def order(self, a, b):
        return "C.order %d %d / " % (a, b) + B.order(self, b=a, a=b)

    def star(self, *args):
        return "C.star %d / " % len(args) + B.star(self, 1, 2, 3)

    def unpack(self, a, b):
        t = (b, a)
        return "C.unpack %d %d / " % (a, b) + B.unpack(self, *t)


class D(B):
    def __init__(self, s=2):
        self.s = s
        super().__init__()

    def dflt(self, a, b=30):
        return "D.dflt %d %d / " % (a, b) + super().dflt(a)


c = C()
print(c.s, c.level)
print(c.dflt(1))
print(c.order(1, 2))
print(c.star(7))
print(c.unpack(1, 2))
d = D()
print(d.s, d.level)
print(d.dflt(5))
