# list() of a list, tuple, range or str iterator drains what is left of it
# (issues/207); it was an unresolved __pyc_tolist__.
print(list(iter((1, 2, 3))), list(iter(range(4))), list(iter("ab")), list(iter([7, 8])))
it2 = iter(range(5))
next(it2)
print(list(it2), list(it2))
