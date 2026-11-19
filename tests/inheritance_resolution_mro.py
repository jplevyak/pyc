# ifa/130 A1: inherited attributes resolve as CPython resolves them -- an
# override SHADOWS, and with several bases the first in the C3 MRO wins.
# Each of these was refused or wrong: the prototype copied every base member
# into the subclass by name and the subclass's body overwrote the slot (a
# union over time), the LAST base's copy won, and the selector dispatch
# matched an inherited method beside a data override, and both bases'
# methods under multiple inheritance ("ambiguous call").
class A1:
    f = 5
class B1(A1):
    def f(self):
        return 7
print(B1().f(), A1().f)

class A2:
    x = 1
class B2(A2):
    x = "s"
print(A2().x, B2().x)

class A3:
    def f(self):
        return 1
class B3(A3):
    f = 5
print(B3().f, A3().f())

class C4:
    x = 1
class D4:
    x = 2
class B4(C4, D4):
    pass
print(B4().x)

class A5:
    x = 1
class C5(A5):
    pass
class D5(A5):
    x = 2
class B5(C5, D5):
    pass
print(B5().x)

class C6:
    def f(self):
        return "C"
class D6:
    def f(self):
        return "D"
class B6(C6, D6):
    pass
print(B6().f(), D6().f())

class A7:
    def f(self):
        return "A"
class C7(A7):
    pass
class D7(A7):
    def f(self):
        return "D"
class B7(C7, D7):
    pass
print(B7().f(), C7().f())

class A8:
    x = 1
    def g(self):
        return self.x
class B8(A8):
    pass
class C8(B8):
    x = 3
print(A8().g(), B8().g(), C8().g())
