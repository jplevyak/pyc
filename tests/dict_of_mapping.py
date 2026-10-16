# issues/178: dict(m) of a mapping copies its items. pyc read the mapping as
# an iterable of pairs, so each key string was split into (key[0], key[1]).
from collections import defaultdict

d = {"ab": 1, "cd": 2}
print(dict(d))

dd = defaultdict(int)
dd["a"] += 1
print(dict(dd))

# The iterable-of-pairs form must keep working.
print(dict([("x", 1), ("y", 2)]))
