# ifa/178: two records of one class, built through ONE constructor contour,
# whose member ends up holding lists of different element types.
#
# `a.labels` is an int list built by `append` inside `__init__`; `b.labels`
# is the caller's str list. Both calls pass the same argument types, so no
# formal confluence splits `__init__`, and both objects are allocated in the
# one `__new__` contour: one Pot CreationSet, whose `labels` member holds
# both lists, so `self.labels[j]` reads {int64, str}. Route 4 declines Pot
# ("confluence only, no demand": the demand is on its member lists), and the
# owner lift from those lists finds one group, because route 4's key follows
# value flow INTO the member and never the store's receiver. tests/none_default_arg_splits_record.py is the same program with
# `labels=None`, which passes only because {list} vs {None} splits
# `__init__` on a formal.


class Pot:
    def __init__(self, n, labels):
        self.n = n
        if n > 2:
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


a = Pot(3, ["zz"])
b = Pot(1, ["abcd"])
print(a.show(), b.show())
