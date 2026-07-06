# pyc shim for the standard `math` module. Elementary functions map
# straight to libc <math.h> (included by pyc_c_runtime.h) via
# __pyc_c_call__, so results match CPython's C-backed math.
pi = 3.141592653589793
e = 2.718281828459045
inf = 1e308 * 10.0
tau = 6.283185307179586

def sqrt(x): return __pyc_c_call__(float, "sqrt", float, x)
def sin(x): return __pyc_c_call__(float, "sin", float, x)
def cos(x): return __pyc_c_call__(float, "cos", float, x)
def tan(x): return __pyc_c_call__(float, "tan", float, x)
def asin(x): return __pyc_c_call__(float, "asin", float, x)
def acos(x): return __pyc_c_call__(float, "acos", float, x)
def atan(x): return __pyc_c_call__(float, "atan", float, x)
def atan2(y, x): return __pyc_c_call__(float, "atan2", float, y, float, x)
def sinh(x): return __pyc_c_call__(float, "sinh", float, x)
def cosh(x): return __pyc_c_call__(float, "cosh", float, x)
def tanh(x): return __pyc_c_call__(float, "tanh", float, x)
def exp(x): return __pyc_c_call__(float, "exp", float, x)
def log(x): return __pyc_c_call__(float, "log", float, x)
def log10(x): return __pyc_c_call__(float, "log10", float, x)
def log2(x): return __pyc_c_call__(float, "log2", float, x)
def pow(x, y): return __pyc_c_call__(float, "pow", float, x, float, y)
def hypot(x, y): return __pyc_c_call__(float, "hypot", float, x, float, y)
def fabs(x): return __pyc_c_call__(float, "fabs", float, x)
def fmod(x, y): return __pyc_c_call__(float, "fmod", float, x, float, y)

# floor/ceil return an integer in Python 3. libc floor/ceil return a
# double (already rounded toward -inf/+inf); int() then truncates it
# to the integral value without changing it.
def floor(x): return int(__pyc_c_call__(float, "floor", float, x))
def ceil(x): return int(__pyc_c_call__(float, "ceil", float, x))
