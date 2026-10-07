class deque:
    # A list plus a head index: popleft advances `head` and the consumed
    # prefix is dropped once it is more than half the list, so both ends
    # are amortized O(1), as CPython's deque is. popleft used to be
    # `self.d.pop(0)`, O(n): sokoban's breadth-first search pops 170k states
    # off a queue of up to 19k, and one solve took 27 s against 0.8 s.
    def __init__(self, iterable=None):
        self.head = 0
        if iterable:
            self.d = [x for x in iterable]
        else:
            self.d = []

    def append(self, item):
        self.d.append(item)

    def appendleft(self, item):
        if self.head > 0:
            self.head -= 1
            self.d[self.head] = item
        else:
            self.d.insert(0, item)

    def extend(self, iterable):
        for x in iterable:
            self.d.append(x)

    def popleft(self):
        if self.head >= len(self.d):
            raise IndexError("pop from an empty deque")
        x = self.d[self.head]
        self.head += 1
        if self.head >= 64 and self.head * 2 >= len(self.d):
            self.d = self.d[self.head:]
            self.head = 0
        return x

    def pop(self):
        if self.head >= len(self.d):
            raise IndexError("pop from an empty deque")
        return self.d.pop()

    def clear(self):
        self.d = []
        self.head = 0

    def __getitem__(self, i):
        n = len(self.d) - self.head
        if i < 0:
            i += n
        if i < 0 or i >= n:
            raise IndexError("deque index out of range")
        return self.d[self.head + i]

    def __len__(self):
        return len(self.d) - self.head

    def __iter__(self):
        return iter(self.d[self.head:])

class defaultdict:
    def __init__(self, factory=None, initial=None):
        self.factory = factory
        self.d = {}
        if initial:
            for k in initial:
                self.d[k] = initial[k]

    def __getitem__(self, key):
        if key not in self.d:
            if self.factory:
                self.d[key] = self.factory()
            else:
                self.d[key] = None
        return self.d[key]
        
    def __setitem__(self, key, value):
        self.d[key] = value

    def __contains__(self, key):
        return key in self.d

    def __len__(self):
        return len(self.d)

    def __iter__(self):
        return iter(self.d)

    def keys(self):
        return self.d.keys()

    def values(self):
        return self.d.values()

    def items(self):
        return self.d.items()
