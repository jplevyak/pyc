# issues/125: `x in y` falls back to the iterable protocol when y defines no
# __contains__, as CPython does: iter(y), compare each item. A class whose
# __iter__ returns self is consumed by `in`, in CPython too (`3 in c` is
# True, then False). It used to be refused: "illegal call argument type".
class Bag:
    def __init__(self):
        self.v = [1, 2, 3]

    def __iter__(self):
        return iter(self.v)


class Counter:
    def __init__(self, n):
        self.i = 0
        self.n = n

    def __iter__(self):
        return self

    def __next__(self):
        if self.i >= self.n:
            raise StopIteration
        self.i += 1
        return self.i


class Own:
    def __contains__(self, x):
        return x == 42


print(2 in Bag(), 5 in Bag(), 7 not in Bag())
c = Counter(5)
print(3 in c, 3 in c, 5 in c)
print(42 in Own(), 1 in Own())
print("b" in "abc", 2 in [1, 2], 3 in {3: 4}, 9 in range(10), 1 in (1, 2))
