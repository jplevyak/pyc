# issues/118: a set of class instances hashes by identity
# (__pyc_any_type__.__hash__, CPython's object.__hash__).
class A:
    def __init__(self, v):
        self.v = v
objs = [A(i) for i in range(5)]
os_ = set(objs)
os_.add(objs[0])
print(len(os_), objs[3] in os_, A(1) in os_)
os_.discard(objs[3])
print(len(os_), objs[3] in os_, objs[4] in os_)
