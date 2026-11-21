# pyc shim for the standard `math` module. Elementary functions map
# straight to libc <math.h> (included by pyc_c_runtime.h) via
# __pyc_c_call__, so results match CPython's C-backed math.
#
# Errors are CPython's (issues/207): a result that is nan for an argument
# that is not, or infinite for a finite one, is CPython's "math domain
# error" ValueError, or "math range error" OverflowError for the functions
# that can overflow (exp, cosh, sinh, expm1, pow, ...). These returned the
# nan or inf silently.
pi = 3.141592653589793
e = 2.718281828459045
inf = 1e308 * 10.0
nan = inf - inf
tau = 6.283185307179586


def isnan(x):
    x = float(x)
    return x != x


def isinf(x):
    x = float(x)
    return x == inf or x == -inf


def isfinite(x):
    x = float(x)
    return not (x != x) and x != inf and x != -inf


# CPython's math_1: the result's class decides the error. Each function
# names its libm call literally: __pyc_c_call__ takes the C name as a
# constant, not a value.
def _math_1(x, r, can_overflow):
    if isnan(r) and not isnan(x):
        raise ValueError("math domain error")
    if isinf(r) and isfinite(x):
        if can_overflow:
            raise OverflowError("math range error")
        raise ValueError("math domain error")
    return r


def sqrt(x):
    x = float(x)
    return _math_1(x, __pyc_c_call__(float, "sqrt", float, x), False)


def sin(x):
    x = float(x)
    return _math_1(x, __pyc_c_call__(float, "sin", float, x), False)


def cos(x):
    x = float(x)
    return _math_1(x, __pyc_c_call__(float, "cos", float, x), False)


def tan(x):
    x = float(x)
    return _math_1(x, __pyc_c_call__(float, "tan", float, x), False)


def asin(x):
    x = float(x)
    return _math_1(x, __pyc_c_call__(float, "asin", float, x), False)


def acos(x):
    x = float(x)
    return _math_1(x, __pyc_c_call__(float, "acos", float, x), False)


def atan(x):
    x = float(x)
    return _math_1(x, __pyc_c_call__(float, "atan", float, x), False)


def sinh(x):
    x = float(x)
    return _math_1(x, __pyc_c_call__(float, "sinh", float, x), True)


def cosh(x):
    x = float(x)
    return _math_1(x, __pyc_c_call__(float, "cosh", float, x), True)


def tanh(x):
    x = float(x)
    return _math_1(x, __pyc_c_call__(float, "tanh", float, x), False)


def asinh(x):
    x = float(x)
    return _math_1(x, __pyc_c_call__(float, "asinh", float, x), False)


def acosh(x):
    x = float(x)
    return _math_1(x, __pyc_c_call__(float, "acosh", float, x), False)


def atanh(x):
    x = float(x)
    return _math_1(x, __pyc_c_call__(float, "atanh", float, x), False)


def exp(x):
    x = float(x)
    return _math_1(x, __pyc_c_call__(float, "exp", float, x), True)


def exp2(x):
    x = float(x)
    return _math_1(x, __pyc_c_call__(float, "exp2", float, x), True)


def expm1(x):
    x = float(x)
    return _math_1(x, __pyc_c_call__(float, "expm1", float, x), True)


def log1p(x):
    x = float(x)
    return _math_1(x, __pyc_c_call__(float, "log1p", float, x), False)


def erf(x):
    x = float(x)
    return _math_1(x, __pyc_c_call__(float, "erf", float, x), False)


def erfc(x):
    x = float(x)
    return _math_1(x, __pyc_c_call__(float, "erfc", float, x), False)


def cbrt(x):
    x = float(x)
    return _math_1(x, __pyc_c_call__(float, "cbrt", float, x), False)


def fabs(x): return __pyc_c_call__(float, "fabs", float, float(x))


def atan2(y, x): return __pyc_c_call__(float, "atan2", float, y, float, x)


# log(x) and the bases: CPython refuses x <= 0 before calling libm.
def _log_arg(x):
    x = float(x)
    if x <= 0.0:
        raise ValueError("math domain error")
    return x


def log(x, base=None):
    # CPython's optional base (mastermind2's `log(p, 2)`). log(x) and
    # log(x, b) bind `base` to different types, so each call gets its own
    # contour and `base is None` folds in both.
    if base is None:
        return __pyc_c_call__(float, "log", float, _log_arg(x))
    num = __pyc_c_call__(float, "log", float, _log_arg(x))
    den = __pyc_c_call__(float, "log", float, _log_arg(base))
    if den == 0.0:
        raise ZeroDivisionError("float division by zero")
    return num / den


def log10(x): return __pyc_c_call__(float, "log10", float, _log_arg(x))
def log2(x): return __pyc_c_call__(float, "log2", float, _log_arg(x))


def pow(x, y):
    x = float(x)
    y = float(y)
    r = __pyc_c_call__(float, "pow", float, x, float, y)
    if isnan(r) and not isnan(x) and not isnan(y):
        raise ValueError("math domain error")
    if isinf(r) and isfinite(x) and isfinite(y):
        if x == 0.0:
            raise ValueError("math domain error")
        raise OverflowError("math range error")
    return r


def hypot(*coordinates):
    t = 0.0
    if len(coordinates) == 2:
        return __pyc_c_call__(float, "hypot", float, coordinates[0], float, coordinates[1])
    for c in coordinates:
        t += float(c) * float(c)
    return sqrt(t)


def fmod(x, y):
    x = float(x)
    y = float(y)
    if isinf(y) and isfinite(x):
        return x
    r = __pyc_c_call__(float, "fmod", float, x, float, y)
    if isnan(r) and not isnan(x) and not isnan(y):
        raise ValueError("math domain error")
    return r


def remainder(x, y):
    x = float(x)
    y = float(y)
    r = __pyc_c_call__(float, "remainder", float, x, float, y)
    if isnan(r) and not isnan(x) and not isnan(y):
        raise ValueError("math domain error")
    return r


def copysign(x, y): return __pyc_c_call__(float, "copysign", float, x, float, y)
def nextafter(x, y): return __pyc_c_call__(float, "nextafter", float, x, float, y)
def ldexp(x, i): return __pyc_c_call__(float, "ldexp", float, x, int, i)


def frexp(x):
    x = float(x)
    return (__pyc_c_call__(float, "_CG_frexp_m", float, x), __pyc_c_call__(int, "_CG_frexp_e", float, x))


def modf(x):
    x = float(x)
    return (__pyc_c_call__(float, "_CG_modf_frac", float, x), __pyc_c_call__(float, "_CG_modf_int", float, x))


def ulp(x):
    x = fabs(float(x))
    if isnan(x) or isinf(x):
        return x
    big = nextafter(x, inf)
    if isinf(big):
        return x - nextafter(x, -inf)
    return big - x


# The float-to-int functions refuse what CPython's int() of a float does.
def _to_int(r):
    if isnan(r):
        raise ValueError("cannot convert float NaN to integer")
    if isinf(r):
        raise OverflowError("cannot convert float infinity to integer")
    return int(r)


# floor/ceil return an integer in Python 3. libc floor/ceil return a
# double (already rounded toward -inf/+inf); int() then truncates it
# to the integral value without changing it.
def floor(x): return _to_int(__pyc_c_call__(float, "floor", float, float(x)))
def ceil(x): return _to_int(__pyc_c_call__(float, "ceil", float, float(x)))
def trunc(x): return _to_int(__pyc_c_call__(float, "trunc", float, float(x)))


# Angle conversions -- pure arithmetic, no libc equivalent. Coerce
# through float() so an int argument (e.g. radians(90)) still yields a
# float, matching CPython.
def radians(x): return float(x) * pi / 180.0
def degrees(x): return float(x) * 180.0 / pi


def isclose(a, b, *, rel_tol=1e-09, abs_tol=0.0):
    a = float(a)
    b = float(b)
    if rel_tol < 0.0 or abs_tol < 0.0:
        raise ValueError("tolerances must be non-negative")
    if a == b:
        return True
    if isinf(a) or isinf(b):
        return False
    diff = fabs(b - a)
    return diff <= fabs(rel_tol * b) or diff <= fabs(rel_tol * a) or diff <= abs_tol


# Integer functions. pyc's int is 64-bit: a result past that wraps, as all
# pyc int arithmetic does (issues/177).
def gcd(*integers):
    g = 0
    for n in integers:
        a = n if n >= 0 else -n
        b = g
        while b:
            t = a % b
            a = b
            b = t
        g = a
    return g


def lcm(*integers):
    l = 1
    for n in integers:
        if n == 0:
            return 0
        a = n if n >= 0 else -n
        l = l // gcd(l, a) * a
    return l


def factorial(n):
    if n < 0:
        raise ValueError("factorial() not defined for negative values")
    r = 1
    for i in range(2, n + 1):
        r *= i
    return r


def perm(n, k=None):
    if n < 0:
        raise ValueError("n must be a non-negative integer")
    if k is None:
        return factorial(n)
    if k < 0:
        raise ValueError("k must be a non-negative integer")
    if k > n:
        return 0
    r = 1
    for i in range(n - k + 1, n + 1):
        r *= i
    return r


def comb(n, k):
    if n < 0:
        raise ValueError("n must be a non-negative integer")
    if k < 0:
        raise ValueError("k must be a non-negative integer")
    if k > n:
        return 0
    if k > n - k:
        k = n - k
    r = 1
    for i in range(1, k + 1):
        r = r * (n - k + i) // i
    return r


def isqrt(n):
    if n < 0:
        raise ValueError("isqrt() argument must be nonnegative")
    if n == 0:
        return 0
    x = n
    y = (x + 1) // 2
    while y < x:
        x = y
        y = (x + n // x) // 2
    return x


def prod(iterable, *, start=1):
    r = start
    for x in iterable:
        r = r * x
    return r


def dist(p, q):
    if len(p) != len(q):
        raise ValueError("both points must have the same number of dimensions")
    t = 0.0
    for i in range(len(p)):
        d = float(p[i]) - float(q[i])
        t += d * d
    return sqrt(t)


def sumprod(p, q):
    t = 0
    for i in range(len(p)):
        t += p[i] * q[i]
    return t


# fsum: CPython's (mathmodule.c), Shewchuk's exact partials with its final
# half-way rounding correction, so the result is the correctly rounded sum.
def fsum(iterable):
    partials = []
    special_sum = 0.0
    inf_sum = 0.0
    for v in iterable:
        x = float(v)
        xsave = x
        i = 0
        for y in partials[:]:
            if fabs(x) < fabs(y):
                t = x
                x = y
                y = t
            hi = x + y
            yr = hi - x
            lo = y - yr
            if lo != 0.0:
                partials[i] = lo
                i += 1
            x = hi
        del partials[i:]
        if x != 0.0:
            if not isfinite(x):
                if isfinite(xsave):
                    raise OverflowError("intermediate overflow in fsum")
                if isinf(xsave):
                    inf_sum += xsave
                special_sum += xsave
                del partials[:]
            else:
                partials.append(x)
    if special_sum != 0.0:
        if isnan(inf_sum):
            raise ValueError("-inf + inf in fsum")
        return special_sum
    hi = 0.0
    n = len(partials)
    lo = 0.0
    if n > 0:
        n -= 1
        hi = partials[n]
        while n > 0:
            x = hi
            n -= 1
            y = partials[n]
            hi = x + y
            yr = hi - x
            lo = y - yr
            if lo != 0.0:
                break
        if n > 0 and ((lo < 0.0 and partials[n - 1] < 0.0) or (lo > 0.0 and partials[n - 1] > 0.0)):
            y = lo * 2.0
            x = hi + y
            yr = x - hi
            if y == yr:
                hi = x
    return hi


# gamma and lgamma are CPython's own (mathmodule.c m_tgamma, m_lgamma), a
# Lanczos approximation, not libm's: their results differ from libm's in
# the last digit, and gamma(0) is a domain error, not an overflow. Errors
# as CPython's math_1a reads errno: EDOM is ValueError, ERANGE is
# OverflowError unless the result is small (an underflow).
_lanczos_g = 6.024680040776729583740234375
_lanczos_g_minus_half = 5.524680040776729583740234375
_lanczos_num = [
    23531376880.410759688572007674451636754734846804940,
    42919803642.649098768957899047001988850926355848959,
    35711959237.355668049440185451547166705960488635843,
    17921034426.037209699919755754458931112671403265390,
    6039542586.3520280050642916443072979210699388420708,
    1439720407.3117216736632230727949123939715485786772,
    248874557.86205415651146038641322942321632125127801,
    31426415.585400194380614231628318205362874684987640,
    2876370.6289353724412254090516208496135991145378768,
    186056.26539522349504029498971604569928220784236328,
    8071.6720023658162106380029022722506138218516325024,
    210.82427775157934587250973392071336271166969580291,
    2.5066282746310002701649081771338373386264310793408]
_lanczos_den = [0.0, 39916800.0, 120543840.0, 150917976.0, 105258076.0, 45995730.0,
                13339535.0, 2637558.0, 357423.0, 32670.0, 1925.0, 66.0, 1.0]
_gamma_integral = [
    1.0, 1.0, 2.0, 6.0, 24.0, 120.0, 720.0, 5040.0, 40320.0, 362880.0,
    3628800.0, 39916800.0, 479001600.0, 6227020800.0, 87178291200.0,
    1307674368000.0, 20922789888000.0, 355687428096000.0,
    6402373705728000.0, 121645100408832000.0, 2432902008176640000.0,
    51090942171709440000.0, 1124000727777607680000.0]
_logpi = 1.144729885849400174143427351353058711647


def _lanczos_sum(x):
    num = 0.0
    den = 0.0
    if x < 5.0:
        i = 12
        while i >= 0:
            num = num * x + _lanczos_num[i]
            den = den * x + _lanczos_den[i]
            i -= 1
    else:
        for i in range(13):
            num = num / x + _lanczos_num[i]
            den = den / x + _lanczos_den[i]
    return num / den


def _sinpi(x):
    y = __pyc_c_call__(float, "fmod", float, fabs(x), float, 2.0)
    n = int(__pyc_c_call__(float, "round", float, 2.0 * y))
    if n == 0:
        r = __pyc_c_call__(float, "sin", float, pi * y)
    elif n == 1:
        r = __pyc_c_call__(float, "cos", float, pi * (y - 0.5))
    elif n == 2:
        r = __pyc_c_call__(float, "sin", float, pi * (1.0 - y))
    elif n == 3:
        r = -__pyc_c_call__(float, "cos", float, pi * (y - 1.5))
    else:
        r = __pyc_c_call__(float, "sin", float, pi * (y - 2.0))
    return copysign(1.0, x) * r


def _raise_errno(err, r):
    if err == 1:
        raise ValueError("math domain error")
    if err == 2 and fabs(r) >= 1.5:
        raise OverflowError("math range error")
    return r


def _cpow(x, y): return __pyc_c_call__(float, "pow", float, x, float, y)
def _cexp(x): return __pyc_c_call__(float, "exp", float, x)
def _clog(x): return __pyc_c_call__(float, "log", float, x)


def gamma(x):
    x = float(x)
    if not isfinite(x):
        if isnan(x) or x > 0.0:
            return x
        raise ValueError("math domain error")
    if x == 0.0:
        raise ValueError("math domain error")
    if x == __pyc_c_call__(float, "floor", float, x):
        if x < 0.0:
            raise ValueError("math domain error")
        if x <= 23.0:
            return _gamma_integral[int(x) - 1]
    absx = fabs(x)
    if absx < 1e-20:
        r = 1.0 / x
        return _raise_errno(2 if isinf(r) else 0, r)
    if absx > 200.0:
        if x < 0.0:
            return 0.0 / _sinpi(x)
        raise OverflowError("math range error")
    y = absx + _lanczos_g_minus_half
    if absx > _lanczos_g_minus_half:
        q = y - absx
        z = q - _lanczos_g_minus_half
    else:
        q = y - _lanczos_g_minus_half
        z = q - absx
    z = z * _lanczos_g / y
    if x < 0.0:
        r = -pi / _sinpi(absx) / absx * _cexp(y) / _lanczos_sum(absx)
        r -= z * r
        if absx < 140.0:
            r /= _cpow(y, absx - 0.5)
        else:
            sqrtpow = _cpow(y, absx / 2.0 - 0.25)
            r /= sqrtpow
            r /= sqrtpow
    else:
        r = _lanczos_sum(absx) / _cexp(y)
        r += z * r
        if absx < 140.0:
            r *= _cpow(y, absx - 0.5)
        else:
            sqrtpow = _cpow(y, absx / 2.0 - 0.25)
            r *= sqrtpow
            r *= sqrtpow
    return _raise_errno(2 if isinf(r) else 0, r)


def lgamma(x):
    x = float(x)
    if not isfinite(x):
        if isnan(x):
            return x
        return inf
    if x == __pyc_c_call__(float, "floor", float, x) and x <= 2.0:
        if x <= 0.0:
            raise ValueError("math domain error")
        return 0.0
    absx = fabs(x)
    if absx < 1e-20:
        return -_clog(absx)
    r = _clog(_lanczos_sum(absx)) - _lanczos_g
    r += (absx - 0.5) * (_clog(absx + _lanczos_g - 0.5) - 1.0)
    if x < 0.0:
        r = _logpi - _clog(fabs(_sinpi(absx))) - _clog(absx) - r
    return _raise_errno(2 if isinf(r) else 0, r)
