# issues/118: dict is a hash index over insertion-ordered storage.
# Insertion order is kept for iteration and printing; overwrite, delete,
# pop, setdefault, get, tuple keys, dict([]) and a large dict.
d = {}
for i in range(20000):
    d[i * 3] = i
n = 0
for i in range(20000):
    if i in d:
        n += d[i]
print(len(d), n, d.get(9, -1), d.get(10, -1))
for k in list(d.keys())[:5]:
    del d[k]
print(len(d), 0 in d, 15 in d, d[15])
s = {"a": 1, "b": 2}
s["c"] = 3
s["a"] = 10
del s["b"]
print(s, len(s), "b" in s, s.pop("c"), s)
s["b"] = 5
print(s, list(s.items()))
t = {(1, 2): "x"}
t[(1, 2)] = "y"
t[(2, 1)] = "z"
print(t, t.setdefault((3, 3), "w"), t.setdefault((1, 2), "q"), len(t))
dd = dict([(1, 2), (3, 4)])
print(dd, dd == {3: 4, 1: 2})
