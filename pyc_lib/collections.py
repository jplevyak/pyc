class deque:
    # A list plus a head index: popleft advances `head` and the consumed
    # prefix is dropped once it is more than half the list, so both ends
    # are amortized O(1), as CPython's deque is. popleft used to be
    # `self.d.pop(0)`, O(n): sokoban's breadth-first search pops 170k states
    # off a queue of up to 19k, and one solve took 27 s against 0.8 s.
    #
    # maxlen is CPython's attribute, None or an int, as given; the logic
    # reads _maxlen, an int with -1 for none (issues/207).
    def __init__(self, iterable=None, maxlen=None):
        self.head = 0
        self.maxlen = maxlen
        self._maxlen = -1 if maxlen is None else maxlen
        if maxlen is not None and maxlen < 0:
            raise ValueError("maxlen must be non-negative")
        self.d = []
        if iterable:
            for x in iterable:
                self.append(x)

    def append(self, item):
        if self._maxlen == 0:
            return
        if self._maxlen > 0 and len(self) >= self._maxlen:
            self.head += 1
        self.d.append(item)

    def appendleft(self, item):
        if self._maxlen == 0:
            return
        if self._maxlen > 0 and len(self) >= self._maxlen:
            self.d.pop()
        if self.head > 0:
            self.head -= 1
            self.d[self.head] = item
        else:
            self.d.insert(0, item)

    def extend(self, iterable):
        for x in iterable:
            self.append(x)

    def extendleft(self, iterable):
        for x in iterable:
            self.appendleft(x)

    def _items(self):
        return self.d[self.head:]

    def _reset(self, items):
        self.d = items
        self.head = 0

    def rotate(self, n=1):
        k = len(self)
        if k == 0:
            return
        n = n % k
        if n:
            items = self._items()
            self._reset(items[k - n:] + items[:k - n])

    def insert(self, i, x):
        if self._maxlen >= 0 and len(self) >= self._maxlen:
            raise IndexError("deque already at its maximum size")
        items = self._items()
        items.insert(i, x)
        self._reset(items)

    def remove(self, value):
        items = self._items()
        for k in range(len(items)):
            if items[k] == value:
                del items[k]
                self._reset(items)
                return
        raise ValueError(repr(value) + " is not in deque")

    def count(self, value):
        n = 0
        for x in self._items():
            if x == value:
                n += 1
        return n

    def index(self, value):
        items = self._items()
        for k in range(len(items)):
            if items[k] == value:
                return k
        raise ValueError(repr(value) + " is not in deque")

    def reverse(self):
        items = self._items()
        items.reverse()
        self._reset(items)

    def copy(self):
        c = deque(None, self.maxlen)
        c.d = self._items()
        return c

    def __copy__(self):
        return self.copy()

    def __contains__(self, value):
        for x in self._items():
            if x == value:
                return True
        return False

    def __setitem__(self, i, value):
        n = len(self)
        if i < 0:
            i += n
        if i < 0 or i >= n:
            raise IndexError("deque index out of range")
        self.d[self.head + i] = value

    def __delitem__(self, i):
        items = self._items()
        n = len(items)
        if i < 0:
            i += n
        if i < 0 or i >= n:
            raise IndexError("deque index out of range")
        del items[i]
        self._reset(items)

    def __reversed__(self):
        items = self._items()
        items.reverse()
        return iter(items)

    def __eq__(self, other):
        return self._items() == other._items()

    def __ne__(self, other):
        return not self.__eq__(other)

    def __iadd__(self, other):
        self.extend(other)
        return self

    def __repr__(self):
        if self._maxlen < 0:
            return "deque(" + repr(self._items()) + ")"
        return "deque(" + repr(self._items()) + ", maxlen=" + str(self._maxlen) + ")"

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

    def __pyc_tobytes__(self):
        # bytes(a_deque_of_ints) (rsync's `bytes(window)`): CPython builds
        # bytes from any iterable of ints; the list path does the checking.
        return self.d[self.head:].__pyc_tobytes__()

class defaultdict:
    def __init__(self, factory=None, initial=None):
        self.factory = factory
        self.d = {}
        if initial:
            for k in initial:
                self.d[k] = initial[k]

    def __getitem__(self, key):
        if key not in self.d:
            # No factory: a missing key is a KeyError, as for a plain
            # dict. It used to insert None, silently.
            if not self.factory:
                raise KeyError(repr(key))  # as dict does: str(KeyError(k)) is repr(k)
            self.d[key] = self.factory()
        return self.d[key]

    def get(self, key, default=None):
        return self.d.get(key, default)

    def __delitem__(self, key):
        del self.d[key]

    # Equal to another defaultdict or a plain dict with the same items, as
    # in CPython (life's `board in history`). It compared by identity, so
    # equal boards never matched and life's process() never returned.
    def __eq__(self, other):
        if isinstance(other, defaultdict):
            return self.d == other.d
        return self.d == other

    def __ne__(self, other):
        return not self.__eq__(other)
        
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

    @property
    def default_factory(self):
        return self.factory

    def setdefault(self, key, default):
        if key not in self.d:
            self.d[key] = default
        return self.d[key]

    def pop(self, key):
        return self.d.pop(key)

    def update(self, other):
        for k in other:
            self.d[k] = other[k]

    def clear(self):
        self.d.clear()

    def copy(self):
        c = defaultdict(self.factory)
        for k in self.d:
            c.d[k] = self.d[k]
        return c

    # CPython prints the factory's repr, `defaultdict(<class 'list'>, {...})`,
    # which pyc cannot produce for a class or function value; refused rather
    # than printed some other way (issues/207).
    def __repr__(self):
        raise NotImplementedError("pyc: repr() of a defaultdict needs its factory's repr (issues/207); print dict(d)")


# Counter, as CPython's: a dict of counts whose missing keys count 0 (issues/207).
# It wraps a dict rather than subclassing one (pyc cannot subclass dict),
# so isinstance(c, dict) is False here; everything else is CPython's.
class Counter:
    def __init__(self, iterable=None, **kwds):
        self.d = {}
        if iterable is not None:
            self.update(iterable)
        if kwds:
            self.update(kwds)

    def update(self, iterable):
        if isinstance(iterable, Counter) or isinstance(iterable, dict):
            for k in iterable:
                self.d[k] = self.d.get(k, 0) + iterable[k]
        else:
            for k in iterable:
                self.d[k] = self.d.get(k, 0) + 1

    def subtract(self, iterable):
        if isinstance(iterable, Counter) or isinstance(iterable, dict):
            for k in iterable:
                self.d[k] = self.d.get(k, 0) - iterable[k]
        else:
            for k in iterable:
                self.d[k] = self.d.get(k, 0) - 1

    def __getitem__(self, key):
        if key in self.d:
            return self.d[key]
        return 0

    def __setitem__(self, key, value):
        self.d[key] = value

    def __delitem__(self, key):
        if key in self.d:
            del self.d[key]

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

    def get(self, key, default=None):
        return self.d.get(key, default)

    def pop(self, key):
        return self.d.pop(key)

    def setdefault(self, key, default):
        if key not in self.d:
            self.d[key] = default
        return self.d[key]

    def clear(self):
        self.d.clear()

    def copy(self):
        c = Counter()
        for k in self.d:
            c.d[k] = self.d[k]
        return c

    def total(self):
        t = 0
        for k in self.d:
            t += self.d[k]
        return t

    # Highest count first; equal counts in insertion order (CPython's
    # sorted(..., reverse=True) and nlargest are both stable).
    def most_common(self, n=None):
        items = [(k, self.d[k]) for k in self.d]
        items.sort(key=_count_of, reverse=True)
        if n is None:
            return items
        if n < 0:
            n = 0
        return items[:n]

    def elements(self):
        out = []
        for k in self.d:
            for i in range(self.d[k]):
                out.append(k)
        return iter(out)

    # Equal when every count is equal, a missing key counting as zero
    # (CPython 3.10+).
    def __eq__(self, other):
        for k in self.d:
            if self.d[k] != other[k]:
                return False
        for k in other:
            if self[k] != other[k]:
                return False
        return True

    def __ne__(self, other):
        return not self.__eq__(other)

    def _keep_positive(self):
        out = Counter()
        for k in self.d:
            if self.d[k] > 0:
                out.d[k] = self.d[k]
        return out

    def __add__(self, other):
        out = Counter()
        for k in self.d:
            v = self.d[k] + other[k]
            if v > 0:
                out.d[k] = v
        for k in other.d:
            if k not in self.d and other.d[k] > 0:
                out.d[k] = other.d[k]
        return out

    def __sub__(self, other):
        out = Counter()
        for k in self.d:
            v = self.d[k] - other[k]
            if v > 0:
                out.d[k] = v
        for k in other.d:
            if k not in self.d and other.d[k] < 0:
                out.d[k] = 0 - other.d[k]
        return out

    def __or__(self, other):
        out = Counter()
        for k in self.d:
            v = self.d[k]
            o = other[k]
            m = o if v < o else v
            if m > 0:
                out.d[k] = m
        for k in other.d:
            if k not in self.d and other.d[k] > 0:
                out.d[k] = other.d[k]
        return out

    def __and__(self, other):
        out = Counter()
        for k in self.d:
            v = self.d[k]
            o = other[k]
            m = v if v < o else o
            if m > 0:
                out.d[k] = m
        return out

    def __pos__(self):
        return self._keep_positive()

    def __neg__(self):
        out = Counter()
        for k in self.d:
            if self.d[k] < 0:
                out.d[k] = 0 - self.d[k]
        return out

    # In place, as CPython's: an alias sees the change.
    def _replace(self, other):
        self.d = other.d
        return self

    def __iadd__(self, other):
        return self._replace(self + other)

    def __isub__(self, other):
        return self._replace(self - other)

    def __ior__(self, other):
        return self._replace(self | other)

    def __iand__(self, other):
        return self._replace(self & other)

    def __repr__(self):
        if not self.d:
            return "Counter()"
        parts = []
        for kv in self.most_common():
            parts.append(repr(kv[0]) + ": " + repr(kv[1]))
        return "Counter({" + ", ".join(parts) + "})"


def _count_of(kv):
    return kv[1]


# OrderedDict, as CPython's: a dict that remembers insertion order (which
# pyc's dict does too) plus move_to_end, popitem(last=) and an order-aware
# == between two OrderedDicts (issues/207). It wraps a dict, so
# isinstance(od, dict) is False here.
class OrderedDict:
    def __init__(self, items=None):
        self.d = {}
        if items is not None:
            self.update(items)

    def update(self, items):
        if isinstance(items, OrderedDict) or isinstance(items, dict):
            for k in items:
                self.d[k] = items[k]
        else:
            for kv in items:
                self.d[kv[0]] = kv[1]

    def __getitem__(self, key):
        if key not in self.d:
            raise KeyError(repr(key))
        return self.d[key]

    def __setitem__(self, key, value):
        self.d[key] = value

    def __delitem__(self, key):
        del self.d[key]

    def __contains__(self, key):
        return key in self.d

    def __len__(self):
        return len(self.d)

    def __iter__(self):
        return iter(self.d)

    def __reversed__(self):
        ks = list(self.d)
        ks.reverse()
        return iter(ks)

    def keys(self):
        return self.d.keys()

    def values(self):
        return self.d.values()

    def items(self):
        return self.d.items()

    def get(self, key, default=None):
        return self.d.get(key, default)

    def pop(self, key):
        return self.d.pop(key)

    def setdefault(self, key, default):
        if key not in self.d:
            self.d[key] = default
        return self.d[key]

    def clear(self):
        self.d.clear()

    def copy(self):
        c = OrderedDict()
        for k in self.d:
            c.d[k] = self.d[k]
        return c

    def popitem(self, last=True):
        if not self.d:
            raise KeyError("dictionary is empty")
        ks = list(self.d)
        k = ks[-1] if last else ks[0]
        v = self.d[k]
        del self.d[k]
        return (k, v)

    def move_to_end(self, key, last=True):
        if key not in self.d:
            raise KeyError(repr(key))
        v = self.d[key]
        if last:
            del self.d[key]
            self.d[key] = v
        else:
            nd = {}
            nd[key] = v
            for k in self.d:
                if not (k == key):
                    nd[k] = self.d[k]
            self.d = nd

    # Order matters between two OrderedDicts; against a plain dict it does
    # not, as in CPython.
    def __eq__(self, other):
        if isinstance(other, OrderedDict):
            return list(self.d.items()) == list(other.d.items())
        return self.d == other

    def __ne__(self, other):
        return not self.__eq__(other)

    def __repr__(self):
        if not self.d:
            return "OrderedDict()"
        return "OrderedDict(" + repr(self.d) + ")"
