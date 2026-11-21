# issues/188 item 1: CFUNCTYPE callbacks. libc calls back into the program
# through a libffi closure, into an exported trampoline (ifa/212) that calls
# the Python function. Every line is CPython's behaviour.
from ctypes import CDLL, CFUNCTYPE, POINTER, sizeof, c_int, c_double, c_size_t

libc = CDLL(None)
CMP = CFUNCTYPE(c_int, POINTER(c_int), POINTER(c_int))
IntArray6 = c_int * 6
libc.qsort.argtypes = [POINTER(c_int), c_size_t, c_size_t, CMP]
libc.qsort.restype = None
libc.bsearch.argtypes = [POINTER(c_int), POINTER(c_int), c_size_t, c_size_t, CMP]
libc.bsearch.restype = POINTER(c_int)

calls = [0]


def py_cmp(a, b):
    calls[0] += 1
    return a[0] - b[0]


def by_last_digit(a, b):
    return a[0] % 10 - b[0] % 10


def scaled(k):
    def cmp(a, b):
        return (a[0] - b[0]) * k
    return cmp


# The ctypes documentation's example.
ia = IntArray6(5, 1, 7, 33, 99, 12)
up = CMP(py_cmp)
libc.qsort(ia, len(ia), sizeof(c_int), up)
print(list(ia), calls[0] > 0)

# Two callback objects of one type alive at once, and a closure.
digits = CMP(by_last_digit)
down = CMP(scaled(-1))
ib = IntArray6(21, 13, 5, 44, 30, 7)
libc.qsort(ib, len(ib), sizeof(c_int), digits)
print(list(ib))
libc.qsort(ib, len(ib), sizeof(c_int), down)
print(list(ib))
libc.qsort(ib, len(ib), sizeof(c_int), up)
print(list(ib))

# bsearch: a pointer result into the array, or NULL.
key = c_int(30)
hit = libc.bsearch(key, ib, len(ib), sizeof(c_int), up)
print(bool(hit))
if hit:
    print(hit[0])
key.value = 31
miss = libc.bsearch(key, ib, len(ib), sizeof(c_int), up)
print(bool(miss))

# Doubles through pointers.
DCMP = CFUNCTYPE(c_int, POINTER(c_double), POINTER(c_double))
dlibc = CDLL(None)                     # pyc's signatures are static: one per binding
dlibc.qsort.argtypes = [POINTER(c_double), c_size_t, c_size_t, DCMP]
dlibc.qsort.restype = None


def dcmp(a, b):
    return (a[0] > b[0]) - (a[0] < b[0])


da = (c_double * 4)(2.5, -1.0, 3.25, 0.5)
dlibc.qsort(da, len(da), sizeof(c_double), DCMP(dcmp))
print(list(da))

