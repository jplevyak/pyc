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
    # times. Its elements are LISTS, not tuples, and that is a deliberate
    # deviation from CPython: N is a runtime value while pyc's tuples are
    # fixed-arity records, so no tuple type can be given here. shedskin
    # sidesteps this by having a variable-length homogeneous `tuple<T>`
    # (its itertools.py is a type stub yielding a 1-tuple whatever
    # `repeat` is, with the real work in C++); pyc has no such type.
    #
    # Lists support iteration, indexing, len and zip, which is what
    # `repeat` is used for in practice -- shedskin_examples/life does
    # `for pos, value in zip(ppos, case)`.
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
            out.append(row)
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
