# Regression: the pyc `math` stdlib shim (pyc_lib/math.py), reachable
# now that module imports work (issue 025 bucket C). Functions map to
# libc <math.h>, so results match CPython.
import math
from math import sqrt, hypot, pi

def main():
    print(math.sqrt(16.0))
    print(sqrt(9.0))
    print(hypot(3.0, 4.0))
    print(math.floor(3.7))
    print(math.ceil(3.2))
    print(math.pow(2.0, 10.0))
    print(int(math.pi * 100.0))
    # radians/degrees (pure arithmetic shims). int()-scale radians to
    # sidestep float-repr differences; degrees of pi is a clean 180.0.
    print(math.degrees(math.pi))
    print(math.degrees(math.pi / 2.0))
    print(int(math.radians(180.0) * 1000.0))
    print(int(math.radians(90.0) * 1000.0))

main()
