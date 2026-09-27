d = {"a": 1, "b": 2, "c": 3}
print(d.pop("b"), len(d), d.get("b", -1), d["c"])
try:
    d.pop("zz")
except KeyError as e:
    print("missing", e)
t = {(1, 2): "x", (3, 4): "y"}
print(t.pop((3, 4)), len(t))
