# ifa/185: tuple.__eq__ dispatches on the operands' ARITY: one
# straight-line body per arity, reached through a runtime length test when
# one variable holds several (ifa/codegen poly_dispatch_arity_plan).
# Covers mixed arities, list-layout tuples (tuple(list)) against records,
# None, and a list (CPython: a tuple never equals a list -- pyc said True
# before this). The mirror, `[1, 2] == (1, 2)`, is list.__eq__ and still
# True (ifa/185). Tuples of one arity but different element types in one
# list are left out: they fail before and after this change (ifa/185).
ts = [(1, 2), (1, 2, 3), (4,), (), tuple([1, 2]), tuple([4]), tuple([])]
for x in ts:
    print([x == y for y in ts], [x != y for y in ts])

m = None
print(m == (1, 2), (1, 2) == m, (1, 2) == [1, 2])
for v in [None, (7, 1)]:
    print(v == (7, 1), (7, 1) == v)

nested = [((1, 2), 3), ((1, 2), 3, 4), ((1,), 3)]
print([a == b for a in nested for b in nested])

d = {}
for k in [(1, 2), (1, 2, 3), tuple([1, 2]), (5,)]:
    d[k] = d.get(k, 0) + 1
print(sorted(d.items()))
s = set([(1, 2), tuple([1, 2]), (2, 1), (1, 2, 3)])
print(len(s), (1, 2) in s, tuple([2, 1]) in s, (9,) in s)
print(ts.index((4,)), ts.count((1, 2)), (1, 2, 3) in ts)
