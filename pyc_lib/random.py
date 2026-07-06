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

def randrange(start, stop):
    return start + int(random() * float(stop - start))

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
