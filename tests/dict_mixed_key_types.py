# issues/018: two dicts with different key types in one program, plus the
# same for sets, mixed value types, and object keys alongside int keys.
# All of this used to fail to compile -- the shared dict/set comparison
# logic read one merged key AVar across every instance.
#
# Verified fixed 2026-08-16. This test exists so it stays fixed; 018's
# remaining, still-broken shape is the bare branch-merged scalar, pinned
# separately by tests/branch_merged_scalar_union.py.
squares = {1: 1, 2: 4, 3: 9}
words = {"a": 1, "b": 2}
print(squares[3], words["b"])

ints = set([1, 2, 3])
strs = set(["x", "y"])
print(2 in ints, "y" in strs)

d1 = {1: "one", 2: "two"}
d2 = {"a": 10, "b": 20}
print(d1[1], d2["a"])

three_a = {1: "i"}
three_b = {"x": 10}
three_c = {1.5: True}
print(three_a[1], three_b["x"], three_c[1.5])
