# issues/185: `not` inverts a narrowing test. FA peeled through `not x`
# (`x.__not__()`) as if it were `x`, so `if not isinstance(l, list):`
# narrowed the wrong branches and the code after the guard had no type.
def size(l):
    if not isinstance(l, list):
        return -1
    return len(l)

def first(t):
    if not not isinstance(t, tuple):
        return t[0]
    return 0

def total(v):
    if not v is None:
        return v + 1
    return 0

print(size([1, 2]), size((1,)), size("ab"))
print(first((7, 8)), first([9]))
print(total(4), total(None))
