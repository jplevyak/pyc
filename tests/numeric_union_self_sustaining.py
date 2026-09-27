# ifa/171: `2*a` and `2*u` (u float) share one int.__mul__ contour from
# pass 0; the {int, float} result feeds back into a/b, every edge then
# carries identical actuals, no split separates them, and numeric
# coercion widens a/b to float. CPython prints ints.
HALF = 1 << 29
QUARTER = 1 << 28
THREEQU = HALF + QUARTER

def encode(s, adaptive):
    for c in s:
        if adaptive:
            pass
    return s

def decode(s):
    b = 1 << 30 ; a = 0
    u = 0 ; v = 1 << 30
    for c in s:
        halfway = u + (v - u) / 2
        if c == '1':
            u = halfway
        a = QUARTER + 1 ; b = THREEQU - 1
        while (a > QUARTER) and (b < THREEQU):
            a = 2*a - HALF;  b = 2*b - HALF ; u = 2*u - HALF ;  v = 2*v - HALF
        w = b - a
    return a, b, w

for s in ["1010", "111"]:
    print(decode(encode(s, 1)))
