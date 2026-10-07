# sorted(iterable, key=None, reverse=False), CPython's signature
# (neural1's `sorted(pairs, reverse=True)`). sorted took `seq` only.
pairs = [(0.5, "b"), (0.9, "a"), (0.5, "a"), (0.1, "c")]
print(sorted(pairs))
print(sorted(pairs, reverse=True))
words = ["pear", "fig", "banana", "kiwi"]
print(sorted(words, key=len))
print(sorted(words, key=len, reverse=True))
print(sorted([3, 1, 2]), sorted((5, 4)), sorted("cab"))
print(sorted([2, 1, 2, 1], reverse=True))
