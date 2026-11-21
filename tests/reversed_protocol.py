# reversed() calls __reversed__ when a class defines it, and reverses a dict's keys (issues/207).
class R:
    def __init__(self, n):
        self.n = n
    def __reversed__(self):
        return iter(range(self.n, 0, -1))
class S:
    def __len__(self):
        return 3
    def __getitem__(self, i):
        return i * 10
d = {"a": 1, "b": 2, "c": 3}
print(list(reversed([1, 2, 3])), list(reversed("abc")), list(reversed((1, 2))), list(reversed(range(4))))
print(list(reversed(R(3))), list(reversed(S())), list(reversed(d)))
for k in reversed(d):
    print(k, d[k])
