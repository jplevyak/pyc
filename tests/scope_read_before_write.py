# issue 025 (mwmatching): a local read before its first assignment,
# guarded by short-circuit, is still a whole-function local -- not a
# global. Two same-shaped functions once collided ("'bd' redefined as
# local") because the first mis-scoped the read as a spurious global.
def best_min(xs):
    first = True
    for d in xs:
        if first or d < bd:   # reads bd before its first write
            first = False
            bd = d
    return bd

def best_max(xs):
    first = True
    for d in xs:
        if first or d > bd:   # same shape, different function
            first = False
            bd = d
    return bd

print(best_min([3, 1, 4, 1, 5]))
print(best_max([3, 1, 4, 1, 5]))
print(best_min([9]))
print(best_max([-2, -7, -1]))
