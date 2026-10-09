# bool(x) is truthiness (__bool__, else __len__), as `if x:` is. It used to
# lower to a C cast, so a non-null container was True whatever its length,
# bool("x") was False, and a user __bool__/__len__ was never called.
# softrender's clipper returns bool(vertices) and indexed an empty list.
class E:
    def __len__(self):
        return 0

class T:
    def __bool__(self):
        return False

class U:
    def __len__(self):
        return 3

a = []
b = [1]
print(bool(a), bool(b), bool(""), bool("x"), bool({}), bool({1: 2}))
print(bool(0), bool(2), bool(0.0), bool(-1.5), bool(None), bool(True))
print(bool(E()), bool(T()), bool(U()), bool(()), bool((1,)), bool(set()))
xs = [1, 2]
xs[:] = []
print(bool(xs), not xs)
