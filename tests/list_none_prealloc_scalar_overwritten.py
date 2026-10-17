# ifa/191: the `[None] * n` preallocation idiom with SCALAR elements, every
# element overwritten before any read. ifa/closed/164 made this work for
# pointer elements, where None is a null pointer; an int element is still
# {None, int}. CPython prints 3 and [0, 2, 4].
n = 3
xs = [None] * n
for i in range(n):
    xs[i] = i * 2
print(xs[1] + 1)
print(xs)
