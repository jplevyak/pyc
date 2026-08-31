# issues/122: list has no __lt__/__le__/__gt__/__ge__.
# CPython compares lists lexicographically; the tuple case is the
# corpus shape (mastermind2's `max([(utility(p), p) for p in plays])`,
# where a tie in the first element falls through to the list).
a = [1, 2]
b = [1, 3]
print(a < b, b < a, a <= a, a > b, b >= a)
print([1, 2] < [1, 2, 0])
pairs = [(1.0, [2, 9]), (1.0, [3, 0])]
print(max(pairs))
