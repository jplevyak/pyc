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
_mti = 625  # _N + 1: not seeded yet (_genrand seeds from OS entropy)

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
    if _mti == _N + 1:
        # Never seeded. CPython seeds from os.urandom when `random` is
        # imported; with no seed this generator ran from an all-zero
        # state and returned 0 forever, so random() was 0.0 (minilight's
        # Russian roulette never ended a path). Seed from OS entropy on
        # first use, the same way seed() does: init_by_array over words.
        key = []
        i = 0
        while i < 8:
            key.append(__pyc_c_call__(int, "_CG_entropy32"))
            i = i + 1
        _init_by_array(key)
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
    _gauss_state[0] = 0.0
    _gauss_have[0] = False

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

def randrange(start, stop=None, step=1):
    # CPython 3.12's, step and errors included (issues/207). The stop-is-None
    # branches keep each call contour monomorphic via nil narrowing (same
    # pattern as min/max key= in __pyc__/05_builtins.py).
    if stop is None:
        if step != 1:
            raise TypeError("Missing a non-None stop argument")
        if start > 0:
            return _randbelow(start)
        raise ValueError("empty range for randrange()")
    width = stop - start
    if step == 1:
        if width > 0:
            return start + _randbelow(width)
        raise ValueError("empty range in randrange(" + str(start) + ", " + str(stop) + ")")
    if step > 0:
        n = (width + step - 1) // step
    elif step < 0:
        n = (width + step + 1) // step
    else:
        raise ValueError("zero step for randrange()")
    if n <= 0:
        raise ValueError("empty range in randrange(" + str(start) + ", " + str(stop) + ", " + str(step) + ")")
    return start + step * _randbelow(n)

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
    # CPython's algorithm, so a seeded run draws the SAME sequence
    # (shedskin_examples/mastermind2's strategies depend on it; the old
    # pool-only version said no corpus program did). Pool-based for a small
    # population, selection-set otherwise, on CPython's `setsize` heuristic.
    # `4 ** ceil(log(3k, 4))` is computed exactly as the smallest power of 4
    # >= 3k: a multiple of 3 is never a power of 4, so the float ceil cannot
    # land on a boundary.
    n = len(population)
    result = []
    setsize = 21
    if k > 5:
        p4 = 1
        while p4 < k * 3:
            p4 = p4 * 4
        setsize += p4
    if n <= setsize:
        pool = []
        for x in population:
            pool.append(x)
        for i in range(k):
            j = _randbelow(n - i)
            result.append(pool[j])
            pool[j] = pool[n - i - 1]
    else:
        selected = set()
        for i in range(k):
            j = _randbelow(n)
            while j in selected:
                j = _randbelow(n)
            selected.add(j)
            result.append(population[j])
    return result


# The rest follow CPython 3.12's random.py, so a seeded run draws the same
# values (issues/207).
def choices(population, weights=None, *, cum_weights=None, k=1):
    n = len(population)
    if cum_weights is None:
        if weights is None:
            nf = float(n)
            return [population[int(random() * nf)] for i in range(k)]
        cw = []
        t = 0
        for w in weights:
            t = t + w
            cw.append(t)
        return _choices_cum(population, cw, k)
    if weights is not None:
        raise TypeError('Cannot specify both weights and cumulative weights')
    return _choices_cum(population, cum_weights, k)


def _choices_cum(population, cum_weights, k):
    n = len(population)
    if len(cum_weights) != n:
        raise ValueError('The number of weights does not match the population')
    total = cum_weights[-1] + 0.0
    if total <= 0.0:
        raise ValueError('Total of weights must be greater than zero')
    if total != total or total == 1e308 * 10.0:
        raise ValueError('Total of weights must be finite')
    hi = n - 1
    out = []
    for i in range(k):
        x = random() * total
        lo = 0
        h = hi
        while lo < h:
            mid = (lo + h) // 2
            if x < cum_weights[mid]:
                h = mid
            else:
                lo = mid + 1
        out.append(population[lo])
    return out


def _clog(x): return __pyc_c_call__(float, "log", float, x)
def _csqrt(x): return __pyc_c_call__(float, "sqrt", float, x)
def _ccos(x): return __pyc_c_call__(float, "cos", float, x)
def _csin(x): return __pyc_c_call__(float, "sin", float, x)
def _cexp(x): return __pyc_c_call__(float, "exp", float, x)


# gauss keeps its second value for the next call, as CPython's does; seed()
# clears it. Held in one-element lists so it is never {None, float}.
_gauss_state = [0.0]
_gauss_have = [False]


def gauss(mu=0.0, sigma=1.0):
    if _gauss_have[0]:
        z = _gauss_state[0]
        _gauss_have[0] = False
    else:
        x2pi = random() * 6.283185307179586
        g2rad = _csqrt(-2.0 * _clog(1.0 - random()))
        z = _ccos(x2pi) * g2rad
        _gauss_state[0] = _csin(x2pi) * g2rad
        _gauss_have[0] = True
    return mu + z * sigma


def normalvariate(mu=0.0, sigma=1.0):
    magic = 4 * _cexp(-0.5) / _csqrt(2.0)
    while True:
        u1 = random()
        u2 = 1.0 - random()
        z = magic * (u1 - 0.5) / u2
        zz = z * z / 4.0
        if zz <= -_clog(u2):
            break
    return mu + z * sigma


def expovariate(lambd=1.0):
    return -_clog(1.0 - random()) / lambd


def lognormvariate(mu, sigma):
    return _cexp(normalvariate(mu, sigma))


# CPython's getrandbits(n * 8).to_bytes(n, 'little'): whole 32-bit words,
# least significant first, then the top bits of one more for the rest.
def randbytes(n):
    out = []
    left = n * 8
    while left >= 32:
        w = _genrand()
        for i in range(4):
            out.append((w >> (8 * i)) & 0xff)
        left -= 32
    if left > 0:
        w = _genrand() >> (32 - left)
        for i in range(left // 8):
            out.append((w >> (8 * i)) & 0xff)
    return bytes(out)
