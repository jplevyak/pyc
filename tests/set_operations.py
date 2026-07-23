# issue 025 (tictactoe): set was missing difference/union/intersection
# (and their -/|/& operators) and __pyc_tolist__ for list(set).
# tictactoe's `set(fields).difference(set([0]))` and `list(players)[0]`.
# Output via sorted() since set iteration order is unspecified.
a = set([1, 2, 3, 4])
b = set([3, 4, 5, 6])

print(sorted(a.difference(b)))
print(sorted(a - b))
print(sorted(a.intersection(b)))
print(sorted(a & b))
print(sorted(a.union(b)))
print(sorted(a | b))

# list(set) conversion
print(sorted(list(a)))
print(len(list(a)))

# the tictactoe pattern: set(list).difference(set([x]))
fields = [1, 2, 0, 1, 2]
players = set(fields).difference(set([0]))
print(len(players))
print(sorted(players))
