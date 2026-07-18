# issue 025 rubik2: augmented assignment to a subscript target
# (`a[i] |= x`) built __setitem__(i, x) directly, discarding the
# operator entirely -- the in-place op's own result was computed but
# never stored anywhere. Covers list, dict, and an attribute-then-
# index chain, plus the loop-carried (non-constant-foldable) case
# that actually broke in rubik2's cube_state.id_/apply_move.
a = [10, 20, 30]
a[0] += 5
a[1] -= 3
a[2] *= 2
print(a[0], a[1], a[2])

b = [7, 8, 9]
for i in range(3):
    b[i] %= 4
print(b[0], b[1], b[2])


class C:
    def __init__(self):
        self.items = [1, 2, 3]


c = C()
c.items[0] |= 8
print(c.items[0])

d = {}
d["x"] = 1
d["x"] += 41
print(d["x"])
