# ifa/213: a truth test on {None, L, R}, R a subclass of L, keeps R's
# inherited __bool__ slot and elides L's, and the shared L.__init__ blind-
# casts between them: 'object layout contract violated'.
class L:
    def __init__(self, level):
        self.level = level
        self.parent = None


class R(L):
    pass


root = R(3)
a = L(4)
a.parent = root
x = a
n = 0
while x:
    n += x.level
    x = x.parent
print(n)
