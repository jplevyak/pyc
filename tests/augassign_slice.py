# issues/043: `a[i:j] op= x` reads the slice, applies the in-place operator to
# it, and writes the result back over the same slice, as CPython does; the
# bounds are evaluated once. It used to store x into the slice unread, so
# `b[1:3] += [10, 20]` acted like `b[1:3] = [10, 20]`.
b = [1, 2, 3, 4, 5]
b[1:3] += [10, 20]
print(b)
c = [1, 2, 3, 4, 5]
c[1:3] *= 2
print(c)
d = [1, 2, 3, 4, 5, 6]
d[-3:] += [7]
print(d)
e = [0, 1, 2, 3, 4, 5, 6, 7]
e[::2] += []
print(e)
calls = []
def idx(k):
    calls.append(k)
    return k
f = [1, 2, 3, 4]
f[idx(1):idx(3)] += [9]
print(f, calls)
class Box:
    def __init__(self):
        self.v = [1, 2, 3]
x = Box()
x.v[0:1] += [5, 6]
print(x.v)
g = [1, 2, 3]
g[1] += 10
print(g)
