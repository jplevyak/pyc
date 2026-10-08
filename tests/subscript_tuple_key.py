# `t[a, b]` is `t[(a, b)]`. The subscript-list node built its children
# but produced no value, so a store keyed `t[1, 2]` was never found by
# `t.get((1, 2))` (sunfish's `self.tp_score[pos, (depth, root)]`).
class E:
    def __init__(self, lower):
        self.lower = lower

t = {}
t[1, 2] = E(3)
print(t.get((1, 2)).lower, t[1, 2].lower, (1, 2) in t, len(t))
m = {}
m["a", 1] = 5
m["a", 1] += 2
print(m["a", 1], m[("a", 1)])
del m["a", 1]
print(len(m))
d = {"x": 1, "y": 2}
d.clear()
d["z"] = 3
print(d, len(d))
