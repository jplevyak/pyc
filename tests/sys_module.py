# Regression: the pyc `sys` stdlib shim (pyc_lib/sys.py, the
# statically-modellable parts). (issue 025 bucket C.)
import sys
from sys import maxsize

def main():
    print(len(sys.argv))          # stub argv: program name only
    print(len(sys.argv) > 1)      # -> False, take default path
    print(maxsize > 1000000000)
    sys.setrecursionlimit(10000)  # no-op
    print("done")

main()
