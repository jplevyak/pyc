# issues/178: dict(x) and d.update(x) copy a MAPPING (d[k] = x[k] for each k
# in x.keys()) and read anything else as an iterable of pairs; which one is
# decided by whether x has keys(), as in CPython. pyc read every argument as
# pairs, so a dict's key strings were split in two ({'ab': 1} -> {'a': 'b'}).
from collections import defaultdict

class M:
    def __init__(self):
        self.store = {"p": 7, "q": 8}
    def keys(self):
        return self.store.keys()
    def __getitem__(self, k):
        return self.store[k] * 10

print(dict({"ab": 1, "cd": 2}))
dd = defaultdict(int)
dd["a"] += 1
print(dict(dd))
print(dict(M()))                 # a user mapping: keys() and __getitem__

# The iterable-of-pairs forms keep working.
print(dict([("x", 1), ("y", 2)]))
print(dict((k, len(k)) for k in ["aa", "b"]))
print(dict(zip(["u", "v"], [3, 4])))

# update() takes the same two forms.
e = {"z": 0}
e.update({"w": 5})
e.update([("t", 6)])
e.update(M())
print(e)
