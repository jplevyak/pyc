# nested-tuple sort: list of (int, (int, int))
b = [(2, (1, 9)), (1, (5, 5)), (1, (5, 4)), (2, (1, 8))]
b.sort()
for y in b:
    print(y[0], y[1][0], y[1][1])
# equality, including across differing arity
print((1, 2) == (1, 2))
print((1, 2) == (1, 3))
print((1, 2) == (1, 2, 3))
print((1, 2) != (1, 2, 3))
print((1, 2, 3) != (1, 2, 3))
# ordering: prefix rule and string fields
print((1, 2) < (1, 2, 3))
print((1, 2, 3) < (1, 2))
print((1, 2, 3) < (1, 3))
print(("a", 1) < ("a", 2))
print(("b",) < ("a", 9))
# nested comparison directly
print((1, (1, 2)) < (1, (1, 3)))
print((1, (2, 0)) < (1, (1, 9)))
