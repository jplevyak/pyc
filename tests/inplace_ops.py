# In-place operators: list and set MUTATE (an alias sees the change), the
# immutable builtins fall back to x = x op y. list *= was `pass` (l became
# None), list += and set |=/&=/-= rebound, and bool/str/bytes/tuple had none.
l = [1, 2]
m = l
l += [3]
l += (4, 5)
print(m, l is m)
l *= 2
print(m)
l *= 0
print(m, len(l))
s = {1, 2, 3}
u = s
s |= {4}
print(sorted(u))
s &= {1, 2, 4}
print(sorted(u))
s -= {1}
print(sorted(u))
s ^= {9, 2}
print(sorted(u))
s ^= s
print(sorted(u), sorted({1, 2} ^ {2, 3}))
t = {5, 6}
t &= t
print(sorted(t))
t -= t
print(sorted(t))
b = True
b &= False
c = True
c |= False
d = True
d ^= True
print(b, c, d)
st = "ab"
st *= 3
st2 = "%d-"
st2 %= 7
print(st, st2)
by = b"x"
by *= 2
print(by)
tp = (1,)
tp += (2,)
print(tp)
