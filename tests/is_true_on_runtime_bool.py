# `x is True` folded to False when x's constants had been stripped (a
# function returning True on one path and False on another): the `is`
# transfer treated the constant True and the runtime bool as disjoint
# CreationSets. shedskin_examples/sat's `assert r is True` failed on a True.
def f(x):
    if x < 0:
        return False
    if x > 10:
        return True
    return True
r = f(3)
print(r is True, r == True, f(-1) is False, f(20) is True, f(-1) is True, f(5) is False)
