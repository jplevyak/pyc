# `[x] * n` is a NEW list even for n <= 0. The runtime used to return NULL
# for n == 0: reads treat NULL as empty, but an in-place mutation through it
# segfaulted (softrender's `self.zbuffer[:] = self.zbuffer_reset` under
# RenderContext(0, 0)), and an alias could not see an append.
n = 0
a = [0.0] * n
b = [1.0] * n
a[:] = b
a.append(2.0)
print(a, [3] * -2, len([4] * 0))
c = [5] * 0
d = c
d.append(6)
print(c, d)
e = [7, 8] * 2
print(e, len(e))
