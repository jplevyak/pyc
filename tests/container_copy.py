# list/dict/set .copy() (ifa/086; sudoku4's `values.copy()`), and copy.copy of
# a dict or set, which shared the original's storage: writing an existing
# key through the copy changed the original.
import copy
d = {"a": 1, "b": 2}
e = d.copy()
e["a"] = 5
e["c"] = 3
f = copy.copy(d)
f["b"] = 7
del f["a"]
s = {1, 2}
t = s.copy()
t.discard(1)
t.add(9)
u = copy.copy(s)
u.add(4)
l = [[1], [2]]
m = l.copy()
m.append([3])
m[1].append(4)
print(d, e, f)
print(sorted(s), sorted(t), sorted(u))
print(l, m)
def search(values, n):
    if n == 0:
        return values
    v2 = values.copy()
    v2["k" + str(n)] = n
    return search(v2, n - 1)
print(search({'a': 1}, 3))
