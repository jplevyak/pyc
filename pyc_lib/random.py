# pyc shim for the standard `random` module. Uses a simple linear
# congruential generator (glibc-style constants) with module-level
# state. This does NOT reproduce CPython's Mersenne Twister sequence,
# so output differs from CPython -- but it is self-consistent and
# deterministic given a seed, which is enough for programs that just
# need randomness (not a specific stream). See issue 025 bucket C.
#
# NB: the explicit float() coercions below are load-bearing on the
# LLVM backend, which currently mis-types `int_fn() / float` as an
# integer op when the int comes from a mutable-global helper (a
# separate pyc bug); float() forces the float operation on both
# backends and reads as intended anyway.
_state = 1

def seed(a):
    global _state
    _state = a & 0x7fffffff
    if _state == 0:
        _state = 1

def _next():
    global _state
    _state = (_state * 1103515245 + 12345) & 0x7fffffff
    return _state

def random():
    return float(_next()) / 2147483648.0

def uniform(a, b):
    return a + (b - a) * random()

def randrange(start, stop=None):
    # CPython's one-arg form randrange(stop) -> [0, stop). The
    # stop-is-None branches keep each call contour monomorphic via
    # nil narrowing (same pattern as min/max key= in
    # __pyc__/05_builtins.py). No step= form yet.
    if stop is None:
        return int(random() * float(start))
    return start + int(random() * float(stop - start))

def triangular(low=0.0, high=1.0, mode=None):
    # Triangular distribution (CPython random.triangular). All
    # arithmetic through explicit float() per this file's NB note --
    # callers pass ints (genetic2: triangular(0, iters, 0)) and the
    # LLVM backend mis-types mixed int/float prims.
    u = random()
    lo = float(low)
    hi = float(high)
    if mode is None:
        c = 0.5
    else:
        c = (float(mode) - lo) / (hi - lo)
    if u > c:
        u = 1.0 - u
        c = 1.0 - c
        t = lo
        lo = hi
        hi = t
    return lo + (hi - lo) * (u * c) ** 0.5

def randint(a, b):
    return a + int(random() * float(b - a + 1))

def choice(seq):
    return seq[int(random() * float(len(seq)))]

def shuffle(x):
    i = len(x) - 1
    while i > 0:
        j = int(random() * float(i + 1))
        t = x[i]
        x[i] = x[j]
        x[j] = t
        i = i - 1

def sample(population, k):
    pool = []
    for x in population:
        pool.append(x)
    
    n = len(pool)
    result = []
    for i in range(k):
        j = int(random() * float(n - i))
        result.append(pool[j])
        pool[j] = pool[n - i - 1]
        
    return result
