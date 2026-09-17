# pyc shim for the standard `random` module.
#
# issues/161: this is MT19937, the generator the Python language actually
# specifies -- not an approximation. `random()` is genrand_res53 over two
# 32-bit draws, seeding is init_by_array over the seed's 32-bit words, and
# randrange/randint/choice/shuffle go through _randbelow's rejection sampling,
# exactly as CPython does. A seeded program therefore produces CPython's
# stream, bit for bit, which is what makes its output comparable at all: 26
# corpus programs import this module and 23 seed deterministically.
#
# It replaces a glibc-style LCG whose stream matched nothing, and whose
# `if _state == 0: _state = 1` guard made seed(0) and seed(1) the SAME stream
# (measured on dijkstra2: 25 of its 50 output lines were duplicates).
#
# NOT plib's mt19937-64.cc: that is the 64-BIT variant (MATRIX_A
# 0xB5026F5AA96619E9, NN 312, MM 156), a different generator with a different
# stream. CPython uses the 32-bit one (0x9908b0df, N 624, M 397).
#
# NB: the explicit float() coercions below are load-bearing on the LLVM
# backend, which mis-types `int_fn() / float` as an integer op when the int
# comes from a mutable-global helper (a separate pyc bug); float() forces the
# float operation on both backends and reads as intended anyway.

_N = 624
_M = 397
_MATRIX_A = 0x9908b0df
_UPPER = 0x80000000
_LOWER = 0x7fffffff

_mt = [0] * 624
_mti = 625

def _init_genrand(s):
    global _mti
    _mt[0] = s & 0xffffffff
    i = 1
    while i < _N:
        prev = _mt[i - 1]
        _mt[i] = (1812433253 * (prev ^ (prev >> 30)) + i) & 0xffffffff
        i = i + 1
    _mti = _N

def _init_by_array(key):
    global _mti
    _init_genrand(19650218)
    klen = len(key)
    i = 1
    j = 0
    k = _N
    if klen > k:
        k = klen
    while k > 0:
        prev = _mt[i - 1]
        _mt[i] = ((_mt[i] ^ ((prev ^ (prev >> 30)) * 1664525)) + key[j] + j) & 0xffffffff
        i = i + 1
        j = j + 1
        if i >= _N:
            _mt[0] = _mt[_N - 1]
            i = 1
        if j >= klen:
            j = 0
        k = k - 1
    k = _N - 1
    while k > 0:
        prev = _mt[i - 1]
        _mt[i] = ((_mt[i] ^ ((prev ^ (prev >> 30)) * 1566083941)) - i) & 0xffffffff
        i = i + 1
        if i >= _N:
            _mt[0] = _mt[_N - 1]
            i = 1
        k = k - 1
    _mt[0] = 0x80000000

def _genrand():
    global _mti
    if _mti >= _N:
        kk = 0
        while kk < _N - _M:
            y = (_mt[kk] & _UPPER) | (_mt[kk + 1] & _LOWER)
            m = 0
            if y & 1:
                m = _MATRIX_A
            _mt[kk] = _mt[kk + _M] ^ (y >> 1) ^ m
            kk = kk + 1
        while kk < _N - 1:
            y = (_mt[kk] & _UPPER) | (_mt[kk + 1] & _LOWER)
            m = 0
            if y & 1:
                m = _MATRIX_A
            _mt[kk] = _mt[kk + (_M - _N)] ^ (y >> 1) ^ m
            kk = kk + 1
        y = (_mt[_N - 1] & _UPPER) | (_mt[0] & _LOWER)
        m = 0
        if y & 1:
            m = _MATRIX_A
        _mt[_N - 1] = _mt[_M - 1] ^ (y >> 1) ^ m
        _mti = 0
    y = _mt[_mti]
    _mti = _mti + 1
    y = y ^ (y >> 11)
    y = y ^ ((y << 7) & 0x9d2c5680)
    y = y ^ ((y << 15) & 0xefc60000)
    y = y ^ (y >> 18)
    return y & 0xffffffff

def seed(a):
    # CPython seeds from the seed's absolute value, little-endian 32-bit words.
    n = a
    if n < 0:
        n = -n
    key = []
    if n == 0:
        key.append(0)
    while n > 0:
        key.append(n & 0xffffffff)
        n = n >> 32
    _init_by_array(key)

def random():
    # genrand_res53: 53 bits of mantissa from two 32-bit draws.
    a = _genrand() >> 5
    b = _genrand() >> 6
    return (float(a) * 67108864.0 + float(b)) / 9007199254740992.0

def getrandbits(k):
    if k <= 32:
        return _genrand() >> (32 - k)
    r = 0
    shift = 0
    left = k
    while left > 0:
        take = left
        if take > 32:
            take = 32
        r = r | ((_genrand() >> (32 - take)) << shift)
        shift = shift + take
        left = left - take
    return r

def _bit_length(n):
    b = 0
    v = n
    while v > 0:
        b = b + 1
        v = v >> 1
    return b

def _randbelow(n):
    # CPython's _randbelow_with_getrandbits: rejection-sample k bits.
    if n <= 0:
        return 0
    k = _bit_length(n)
    r = getrandbits(k)
    while r >= n:
        r = getrandbits(k)
    return r

def uniform(a, b):
    return a + (b - a) * random()

def randrange(start, stop=None):
    # CPython's one-arg form randrange(stop) -> [0, stop). The stop-is-None
    # branches keep each call contour monomorphic via nil narrowing (same
    # pattern as min/max key= in __pyc__/05_builtins.py). No step= form yet.
    if stop is None:
        return _randbelow(start)
    return start + _randbelow(stop - start)

def randint(a, b):
    return a + _randbelow(b - a + 1)

def choice(seq):
    return seq[_randbelow(len(seq))]

def shuffle(x):
    # CPython: for i in reversed(range(1, len(x))): j = _randbelow(i+1); swap.
    i = len(x) - 1
    while i > 0:
        j = _randbelow(i + 1)
        t = x[i]
        x[i] = x[j]
        x[j] = t
        i = i - 1

def triangular(low=0.0, high=1.0, mode=None):
    # Triangular distribution (CPython random.triangular). All arithmetic
    # through explicit float() per this file's NB note -- callers pass ints
    # (genetic2: triangular(0, iters, 0)) and the LLVM backend mis-types mixed
    # int/float prims.
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

def sample(population, k):
    # NOT stream-identical to CPython, which switches between a selection-set
    # and a pool algorithm on a size heuristic. The draws below come from the
    # same generator, so a seeded run is deterministic and reproducible, but
    # the SEQUENCE differs from CPython's. Left as-is deliberately: matching it
    # exactly needs CPython's setsize branch, and no corpus program depends on
    # sample()'s order.
    pool = []
    for x in population:
        pool.append(x)
    n = len(pool)
    result = []
    for i in range(k):
        j = _randbelow(n - i)
        result.append(pool[j])
        pool[j] = pool[n - i - 1]
    return result
