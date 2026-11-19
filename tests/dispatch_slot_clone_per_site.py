# ifa/121: a dispatch arm must call the clone its SITE chose. A.m has two
# clones, one per argument type (int at one site, str at the other), and
# an object's method slot holds only one, filled per constructor. Both
# call sites dispatch over {A, B} through that slot, so before the
# dispatch-arm plans one of them called the other site's clone: o.m(21)
# ran the str clone on an int and printed a pointer. Now each arm whose
# slot may hold another clone calls the site's clone directly.
import sys
class A:
    def m(self, x):
        return x + x
class B:
    def m(self, x):
        return x
def pick(n):
    return A() if n > 0 else B()
o = pick(len(sys.argv))
p = pick(len(sys.argv))
print(o.m(21))
print(p.m("ab"))
