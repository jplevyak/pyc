# A class attribute read or written through a subscript on the class name
# (`L.k[i]`). The subscript trailer flushed the pending member on the class
# Sym instead of its meta instance, so every form below was `unresolved
# member 'k' of class 'L'` -- tarsalzp's `Lg2.lgLut[value]`.
class L(object):
    k = [3, 4]
    s = "ab"
    lut = [-1]
    for i in range(1, 256):
        lut.append(1 + lut[i // 2])

    @staticmethod
    def ilog(v):
        return L.lut[v]


print(L.k[0], L.s[1], L.s[0:1])
L.k[0] = 7
L.k[1:] = [9, 10]
print(L.k)
print(L.ilog(5), L.ilog(255))
