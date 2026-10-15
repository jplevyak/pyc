# ifa/134: a branch that decides which list a record's member gets must fold
# per contour, or the stores of BOTH arms reach both records.
#
# Both calls pass the same argument types, and pyc already gives each object
# its own Pot CreationSet and each call its own `__init__` contour. What
# separates them is only the VALUE of `n`: 3 takes the first arm (an int list
# filled by `append`), 1 the second (the caller's str list). `n > 2` runs in
# `int.__gt__`, one contour shared by both calls, so its `self` is not a
# constant there and the branch does not fold in either `__init__`. The
# second object's `__init__` then stores both lists into `self.labels` and
# appends ints through it into the str list, and `self.labels[j]` reads
# {int64, str}. The demand is that violation; CONST_DEMAND walks from the
# polluted list element to the `append` that wrote it and to the branch
# guarding that call, and splits `int.__gt__` on the constants its callers
# disagree on (1 and 3). (Filed first as ifa/178's writer split; it is not.)


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
