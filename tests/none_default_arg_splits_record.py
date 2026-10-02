# ifa/178: one class built through one constructor from two creation points,
# one with `labels=None` and one with a list of str. With None stripped from
# the type projection, `__init__` never split between the two calls, so one
# Pot CreationSet held both lists and `self.labels[j]` was refused as
# {int64, str}. Keeping None in `->type` makes `{list}` vs `{None}` at
# `labels` a type confluence: `__init__` splits, and so does Pot.
class Pot:
    def __init__(self, n, labels=None):
        self.n = n
        if labels == None:
            self.labels = []
            for i in range(n):
                self.labels.append(i * 10)
        else:
            self.labels = labels
    def show(self):
        r = 0
        for j in range(self.n):
            r += len(str(self.labels[j]))
        return r
class Wave:
    def __init__(self):
        self.pot = Pot(3)
    def total(self):
        return self.pot.show()
def run():
    w = Wave()
    print(w.total())
    w.pot = Pot(1, ["abcd"])
    print(w.total())
run()
