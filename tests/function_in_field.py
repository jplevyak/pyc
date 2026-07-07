# Regression: first-class functions stored in instance attributes
# (issue 025). Python's rule: a function found on the CLASS binds as
# a method; a function stored as an INSTANCE attribute does NOT bind
# -- self.cf(3, 1) calls cf(3, 1), not cf(self, 3, 1). The period
# prim previously bound every function-valued field, so calls
# through stored callbacks dispatched with the object inserted and
# matched nothing (timsort's comparefn pattern).
def mycmp(a, b):
    return a - b

def double(x):
    return x * 2

class T:
    def __init__(self, cf):
        self.cf = cf

    def go(self):
        return self.cf(3, 1)          # direct call through the field

    def via_local(self):
        f = self.cf                    # read, then call
        return f(10, 4)

class WithClassAttr:
    v = 7
    get = lambda y: y.v               # CLASS attribute: still binds

def main():
    t = T(mycmp)
    print(t.go())          # 2
    print(t.via_local())   # 6
    u = T(double)          # different function, same field... separate instance
    w = WithClassAttr()
    print(w.get())         # 7 (bound: y = w)

main()
