# issues/166: an extended slice STORE -- `a[i:j:k] = v` with k != 1.
#
# `__pyc_setslice__` was handed the step by the frontend and dropped it,
# and the runtime had no parameter to receive it, so every strided store
# ran as the contiguous splice `a[i:i+len(v)] = v`: `a[3::4] = [0]*5` on a
# 20-element list replaced elements 3..7 and TRUNCATED the list to 8,
# silently, exit 0. That was shedskin_examples/sieve's wrong answer --
# its Sieve of Eratostenes is built on `sieve[bottom::si] = [0]*n`, so it
# printed `nprimes: 4` where CPython prints 664579.
#
# The read side was already correct, so this pins both halves together:
# a strided store must not resize, and a CONTIGUOUS one still must.
a = list(range(20))
a[3::4] = [0, 0, 0, 0, 0]
print(a)

b = list(range(10))
b[2::3] = [9, 9, 9]
print(b)

c = list(range(10))
c[0::2] = [7] * 5
print(c)

# negative steps, the case the read side already handled
d = list(range(10))
d[::-2] = [1, 2, 3, 4, 5]
print(d)

e = list(range(10))
e[8:2:-2] = [11, 12, 13]
print(e)

# step 1 stated explicitly is CONTIGUOUS and may resize
f = list(range(6))
f[1:5:1] = [90, 91]
print(f)

# no step at all: unchanged behaviour, resizes
g = list(range(6))
g[2:4] = [50, 51, 52]
print(g)

# contiguous delete, which lowers through the same runtime helper
h = list(range(6))
del h[1:3]
print(h)

# a strided read, to keep the two sides pinned to each other
i = list(range(12))
print(i[1::3], i[::-4], i[9:1:-3])

# the sieve shape itself, reduced
def eratostenes(n):
    if n <= 2:
        return []
    sieve = list(range(3, n, 2))
    top = len(sieve)
    for si in sieve:
        if si:
            bottom = (si * si - 3) // 2
            if bottom >= top:
                break
            sieve[bottom::si] = [0] * -((bottom - top) // si)
    return [2] + [el for el in sieve if el]


print(len(eratostenes(1000)))
print(eratostenes(30))
