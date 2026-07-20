# ifa/issues/058 (shedskin kanoodle.py): a method INHERITED (not
# overridden) by several concrete subclasses, called on a receiver
# whose static type is a union of those subclasses (a genuinely
# polymorphic list), goes through cg.cc's write_c_prim classtag
# dispatch branch -- distinct from tests/multi_candidate_dispatch.py,
# where each class defines its OWN override of the method. That
# branch used to hardcode the emitted function pointer type to a
# single `void*` (self) parameter and pass only the receiver, silently
# dropping every other live argument (translate's `v` here) -- the
# callee read whatever happened to be sitting in the relevant
# register/stack slot for its second parameter, undefined behavior
# that manifested as a segfault in kanoodle.py's real Omino.translate
# call but does NOT reliably reproduce as visibly wrong output in
# this minimal shape (confirmed: the exact same broken codegen
# pattern is present with -O0 or -O3, but happens to read back the
# right value by ABI/register-allocation luck at this scale -- this
# test is real positive coverage, not a crash-discriminating
# regression test; kanoodle.py itself, in the shedskin corpus, is the
# reliable repro, verified separately via the corpus sweep).
class Base:
    def translate(self, v):
        return [self.x + v[0], self.y + v[1]]

class A(Base):
    def __init__(self):
        self.x = 0
        self.y = 0

class B(Base):
    def __init__(self):
        self.x = 1
        self.y = 10

class C(Base):
    def __init__(self):
        self.x = 2
        self.y = 20

class D(Base):
    def __init__(self):
        self.x = 3
        self.y = 30

class E(Base):
    def __init__(self):
        self.x = 4
        self.y = 40

class F(Base):
    def __init__(self):
        self.x = 5
        self.y = 50

class G(Base):
    def __init__(self):
        self.x = 6
        self.y = 60

class H(Base):
    def __init__(self):
        self.x = 7
        self.y = 70

class I(Base):
    def __init__(self):
        self.x = 8
        self.y = 80

class J(Base):
    def __init__(self):
        self.x = 9
        self.y = 90

class K(Base):
    def __init__(self):
        self.x = 10
        self.y = 100

items = [A(), B(), C(), D(), E(), F(), G(), H(), I(), J(), K()]
for item in items:
    print(item.translate([100, 200]))
