# A range is re-iterable (CPython's range_iterator is a separate object).
# pyc's range used to be its own iterator, so iterating one twice gave
# nothing the second time: product(range(3), range(3)) yielded 3 pairs.
def prod(A, B):
    result = []
    for a in A:
        for b in B:
            result.append((a, b))
    return result

print(prod(range(3), range(3)))
r = range(3)
print(list(r), list(r), 2 in r, len(r))
for a in r:
    for b in r:
        print(a, b, end=" ")
print()
it = iter(r)
print(next(it))
for x in it:
    print(x)
print([x for x in range(0)], [x * 2 for x in range(5, 0, -2)])
