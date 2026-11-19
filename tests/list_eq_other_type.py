# issues/185: a list is never equal to a non-list. `list.__eq__` compared
# elements whatever its argument was, so `[1, 2] == (1, 2)` was True.
print([1, 2] == (1, 2))
print((1, 2) == [1, 2])
print([1, 2] != (1, 2))
print(["a", "b"] == "ab")
print([1] == [1], [1] != [2], [] == [])
x = []
x.append(3)
print(x == [3], x == (3,))
