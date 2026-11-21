# `del l[i]` with a negative index deletes from the end; out of range, or
# `del d[k]` for a missing key, raises. `del l[-1]` deleted nothing, and the
# other two did nothing silently (issues/205).
cl = ['a', ' ', 'b', ' ']
del cl[-1]
print(cl)
del cl[0]
print(cl)
del cl[len(cl) - 1]
print(cl)
l = [1, 2, 3]
del l[-3]
print(l)
try:
    del l[5]
except IndexError as e:
    print("IndexError", e)
try:
    del l[-3]
except IndexError as e:
    print("IndexError", e)
d = {"a": 1, "b": 2}
try:
    del d["c"]
except KeyError as e:
    print("KeyError", e)
del d["a"]
print(d)
e2 = {}
try:
    del e2[1]
except KeyError as e:
    print("KeyError", e)
