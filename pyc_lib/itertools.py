class count:
    def __init__(self, start=0, step=1):
        self.n = start
        self.step = step
        
    def __iter__(self):
        return self

    def __pyc_more__(self):
        # issues/116: pyc's for-loop protocol is peek-then-fetch
        # (__iter__ / __pyc_more__ / __next__), not CPython's
        # fetch-until-StopIteration. `object.__pyc_more__` returns
        # False, so a class defining only __iter__/__next__ -- the
        # standard Python protocol, which this one did -- iterates ZERO
        # times in a `for`, silently and with no diagnostic.
        # `for j in count(...)` printed nothing at all; sunfish's
        # gen_moves scans every ray with one, so every ray was empty.
        # A count is infinite, so there is always a next value.
        return True

    def __next__(self):
        v = self.n
        self.n += self.step
        return v

def product(A, B=None, C=None, D=None, repeat=1):
    # issues/103: `repeat=N` is the cartesian product of A with itself N
    # times. N is a runtime value, so each element is built with
    # `tuple(row)`, which is a runtime-length tuple (PYC_MAKESEQ, issues/110)
    # -- a real tuple, as in CPython. issues/171 #5: these used to be LISTS,
    # an observable deviation (printing, hashing, `==` against a tuple).
    if repeat != 1:
        items = []
        for a in A:
            items.append(a)
        n = len(items)
        total = 1
        for _ in range(repeat):
            total = total * n
        # pows[j] == n**j, so the leftmost position varies slowest --
        # matching CPython's ordering (000, 001, 010, 011, ...).
        pows = []
        p = 1
        for _ in range(repeat):
            pows.append(p)
            p = p * n
        out = []
        for k in range(total):
            row = []
            for i in range(repeat):
                row.append(items[(k // pows[repeat - 1 - i]) % n])
            out.append(tuple(row))
        return out
    result = []
    if B is None:
        for a in A:
            result.append((a,))
    elif C is None:
        for a in A:
            for b in B:
                result.append((a, b))
    elif D is None:
        for a in A:
            for b in B:
                for c in C:
                    result.append((a, b, c))
    else:
        for a in A:
            for b in B:
                for c in C:
                    for d in D:
                        result.append((a, b, c, d))
    return result


# The rest follow the pure-Python equivalents in CPython's itertools
# documentation, as generators, so each consumes its input exactly as
# CPython's does (issues/207). Each returns an iterator, not a list.

class chain:
    def __init__(self, *iterables):
        self._src = list(iterables)

    @classmethod
    def from_iterable(cls, iterables):
        c = cls()
        c._src = list(iterables)
        return c

    def __iter__(self):
        for it in self._src:
            for x in it:
                yield x


# islice(it, stop) or islice(it, start, stop[, step]); stop None runs to the
# end. The arguments are checked when islice is called, as CPython's are,
# and the iteration is a separate generator.
def islice(iterable, *args):
    start = 0
    step = 1
    if len(args) == 1:
        stop = args[0]
    else:
        start = args[0]
        stop = args[1]
        if len(args) > 2:
            step = args[2]
    if stop is not None and stop < 0:
        raise ValueError("Stop argument for islice() must be None or an integer: 0 <= x <= sys.maxsize.")
    if start < 0:
        raise ValueError("Indices for islice() must be None or an integer: 0 <= x <= sys.maxsize.")
    if step < 1:
        raise ValueError("Step for islice() must be a positive integer or None.")
    return _islice(iter(iterable), start, stop, step)


def _islice(it, start, stop, step):
    i = 0
    nexti = start
    if stop is not None and nexti >= stop:
        # consume up to start, as CPython does
        while i < start:
            got = False
            for x in it:
                got = True
                break
            if not got:
                return
            i += 1
        return
    for x in it:
        if i == nexti:
            yield x
            nexti += step
            if stop is not None and nexti >= stop:
                # consume to stop
                i += 1
                while i < stop:
                    got = False
                    for y in it:
                        got = True
                        break
                    if not got:
                        return
                    i += 1
                return
        i += 1


def repeat(obj, times=None):
    if times is None:
        while True:
            yield obj
    else:
        for i in range(times):
            yield obj


def cycle(iterable):
    saved = []
    for element in iterable:
        yield element
        saved.append(element)
    while saved:
        for element in saved:
            yield element


# Written so that no variable holds None and then a value (a {None, int}
# local has no representation): the first total comes from `initial` or
# from the first element, in two loops.
def _accumulate_from(it, total, func):
    yield total
    for element in it:
        if func is None:
            total = total + element
        else:
            total = func(total, element)
        yield total


def accumulate(iterable, func=None, *, initial=None):
    it = iter(iterable)
    if initial is not None:
        for v in _accumulate_from(it, initial, func):
            yield v
        return
    for x in it:
        for v in _accumulate_from(it, x, func):
            yield v
        return


# groupby, as CPython's C implementation behaves: one shared lookahead, and
# a group's iterator ends as soon as the outer iterator moves on. The state
# is made from the first element, so its value and key fields never hold
# None before a value.
class _GroupState:
    def __init__(self, it, value, key):
        self.it = it
        self.done = False
        self.have = True
        self.gen = 0
        self.value = value
        self.key = key


def _gb_fetch(st, key):
    for x in st.it:
        st.value = x
        st.key = x if key is None else key(x)
        st.have = True
        return
    st.done = True


def _grouper(st, key, target, gen):
    while True:
        if st.gen != gen:
            return
        if not st.have:
            _gb_fetch(st, key)
            if st.done:
                return
        if not (st.key == target):
            return
        st.have = False
        yield st.value


def _groupby_from(st, key):
    target = st.key
    st.gen += 1
    yield (target, _grouper(st, key, target, st.gen))
    while True:
        if not st.have:
            _gb_fetch(st, key)
            if st.done:
                return
        if not (st.key == target):
            target = st.key
            st.gen += 1
            yield (target, _grouper(st, key, target, st.gen))
        else:
            st.have = False


def groupby(iterable, key=None):
    it = iter(iterable)
    for x in it:
        st = _GroupState(it, x, x if key is None else key(x))
        for kg in _groupby_from(st, key):
            yield kg
        return


def zip_longest(*iterables, fillvalue=None):
    its = [iter(x) for x in iterables]
    n = len(its)
    if n == 0:
        return
    done = [False] * n
    active = n
    while True:
        row = []
        for k in range(n):
            got = False
            if not done[k]:
                for x in its[k]:
                    row.append(x)
                    got = True
                    break
                if not got:
                    done[k] = True
                    active -= 1
                    if active == 0:
                        return
            if not got:
                row.append(fillvalue)
        yield tuple(row)


def pairwise(iterable):
    it = iter(iterable)
    for a in it:
        for b in it:
            yield (a, b)
            a = b
        return


def batched(iterable, n):
    if n < 1:
        raise ValueError('n must be at least one')
    return _batched(iterable, n)


def _batched(iterable, n):
    batch = []
    for x in iterable:
        batch.append(x)
        if len(batch) == n:
            yield tuple(batch)
            batch = []
    if batch:
        yield tuple(batch)


def compress(data, selectors):
    sel = iter(selectors)
    for d in data:
        got = False
        for s in sel:
            got = True
            if s:
                yield d
            break
        if not got:
            return


def dropwhile(predicate, iterable):
    dropping = True
    for x in iterable:
        if dropping and predicate(x):
            continue
        dropping = False
        yield x


def takewhile(predicate, iterable):
    for x in iterable:
        if not predicate(x):
            return
        yield x


def filterfalse(predicate, iterable):
    for x in iterable:
        if predicate is None:
            if not x:
                yield x
        elif not predicate(x):
            yield x


def starmap(function, iterable):
    for args in iterable:
        yield function(*args)


def permutations(iterable, r=None):
    pool = list(iterable)
    n = len(pool)
    if r is None:
        r = n
    if r > n:
        return
    indices = list(range(n))
    cycles = list(range(n, n - r, -1))
    yield tuple([pool[i] for i in indices[:r]])
    while n:
        found = False
        for i in reversed(range(r)):
            cycles[i] -= 1
            if cycles[i] == 0:
                indices[i:] = indices[i + 1:] + indices[i:i + 1]
                cycles[i] = n - i
            else:
                j = cycles[i]
                t = indices[i]
                indices[i] = indices[-j]
                indices[-j] = t
                yield tuple([pool[k] for k in indices[:r]])
                found = True
                break
        if not found:
            return


def combinations(iterable, r):
    pool = list(iterable)
    n = len(pool)
    if r > n:
        return
    indices = list(range(r))
    yield tuple([pool[i] for i in indices])
    while True:
        found = -1
        for i in reversed(range(r)):
            if indices[i] != i + n - r:
                found = i
                break
        if found < 0:
            return
        i = found
        indices[i] += 1
        for j in range(i + 1, r):
            indices[j] = indices[j - 1] + 1
        yield tuple([pool[k] for k in indices])


def combinations_with_replacement(iterable, r):
    pool = list(iterable)
    n = len(pool)
    if not n and r:
        return
    indices = [0] * r
    yield tuple([pool[i] for i in indices])
    while True:
        found = -1
        for i in reversed(range(r)):
            if indices[i] != n - 1:
                found = i
                break
        if found < 0:
            return
        i = found
        v = indices[i] + 1
        for j in range(i, r):
            indices[j] = v
        yield tuple([pool[k] for k in indices])
