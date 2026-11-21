# pyc shim for the standard `bisect` module: CPython's algorithms, `key=`
# included (issues/207). hi=None searches to the end.

def bisect_right(a, x, lo=0, hi=None, *, key=None):
    if lo < 0:
        raise ValueError('lo must be non-negative')
    if hi is None:
        hi = len(a)
    while lo < hi:
        mid = (lo + hi) // 2
        if key is None:
            if x < a[mid]:
                hi = mid
            else:
                lo = mid + 1
        else:
            if x < key(a[mid]):
                hi = mid
            else:
                lo = mid + 1
    return lo


def bisect_left(a, x, lo=0, hi=None, *, key=None):
    if lo < 0:
        raise ValueError('lo must be non-negative')
    if hi is None:
        hi = len(a)
    while lo < hi:
        mid = (lo + hi) // 2
        if key is None:
            if a[mid] < x:
                lo = mid + 1
            else:
                hi = mid
        else:
            if key(a[mid]) < x:
                lo = mid + 1
            else:
                hi = mid
    return lo


def insort_right(a, x, lo=0, hi=None, *, key=None):
    if key is None:
        lo = bisect_right(a, x, lo, hi)
    else:
        lo = bisect_right(a, key(x), lo, hi, key=key)
    a.insert(lo, x)


def insort_left(a, x, lo=0, hi=None, *, key=None):
    if key is None:
        lo = bisect_left(a, x, lo, hi)
    else:
        lo = bisect_left(a, key(x), lo, hi, key=key)
    a.insert(lo, x)


bisect = bisect_right
insort = insort_right
