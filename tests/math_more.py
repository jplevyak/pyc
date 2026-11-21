# math: integer functions, isclose, fsum, frexp/modf, CPython's gamma/lgamma, and CPython's domain and range errors (issues/207).
import math
print(math.gcd(12, 18), math.gcd(), math.gcd(-4, 6, 10), math.lcm(4, 6), math.lcm(), math.lcm(3, 0))
print(math.factorial(0), math.factorial(10), math.comb(10, 3), math.comb(3, 5), math.perm(5, 2), math.perm(4), math.perm(3, 4))
print(math.isqrt(0), math.isqrt(15), math.isqrt(16), math.isqrt(10**12 + 1), math.prod([2, 3, 4]), math.prod([1.5, 2.0], start=2.0))
print(math.isclose(1.0, 1.0 + 1e-10), math.isclose(1.0, 1.1), math.isclose(0.0, 1e-10, abs_tol=1e-9), math.isclose(math.inf, math.inf))
print(math.isnan(math.nan), math.isnan(1.0), math.isinf(-math.inf), math.isfinite(1e308), math.isfinite(math.inf))
print(math.trunc(2.7), math.trunc(-2.7), math.floor(-2.5), math.ceil(-2.5), math.copysign(3.0, -0.0), math.fabs(-2))
print(math.frexp(8.0), math.frexp(-0.3), math.frexp(0.0), math.modf(3.25), math.modf(-3.25), math.ldexp(0.5, 4))
print(math.log1p(1e-10), math.expm1(1e-10), math.erf(0.5), math.erfc(0.5), math.gamma(5), math.lgamma(10), math.cbrt(27), math.exp2(10))
print(math.asinh(1.0), math.acosh(2.0), math.atanh(0.5), math.hypot(3, 4), math.hypot(1, 2, 2), math.dist((0, 0), (3, 4)))
print(math.fsum([0.1] * 10), math.fsum([1e100, 1.0, -1e100, 1e-100, 1e50, -1.0, -1e50]))
print(math.remainder(10, 3), math.fmod(10, 3), math.fmod(-10, 3), math.fmod(5.0, math.inf), math.ulp(1.0), math.nextafter(1.0, 2.0))
print(math.log(8, 2), math.log(math.e), math.log10(1000), math.log2(1024), math.pow(2, 10), math.sqrt(16), math.sumprod([1, 2], [3, 4]))
def err(f, x):
    try:
        print(f(x))
    except ValueError as e:
        print("ValueError", e)
    except OverflowError as e:
        print("OverflowError", e)
err(math.sqrt, -1)
err(math.log, 0)
err(math.log, -1.5)
err(math.acos, 2)
err(math.exp, 1000)
err(math.cosh, 1000)
err(math.factorial, -1)
err(math.isqrt, -5)
err(math.floor, math.inf)
err(math.trunc, math.nan)
err(math.gamma, 0)
err(math.log10, 0)
try:
    print(math.pow(0, -1))
except ValueError as e:
    print("ValueError", e)
try:
    print(math.pow(10, 400))
except OverflowError as e:
    print("OverflowError", e)
try:
    print(math.fsum([math.inf, -math.inf]))
except ValueError as e:
    print("ValueError", e)
print(math.fsum([math.inf, 1.0]), math.isnan(math.fsum([math.nan])))
for v in [0.5, 1.5, 2.5, 7.3, 25.0, 30.5, -0.5, -2.5, 1e-25, 150.0, 171.5, -150.5]:
    print(v, math.gamma(v), math.lgamma(v))
for v in [-1.0, 0.0, 300.0]:
    err(math.gamma, v)
err(math.lgamma, -2.0)
err(math.lgamma, 1e308)
print(math.lgamma(-1e-25))
