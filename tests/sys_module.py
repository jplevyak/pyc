# Regression: the pyc `sys` stdlib shim (pyc_lib/sys.py, the
# statically-modellable parts). (issue 025 bucket C.)
import sys
from sys import maxsize

def main():
    print(len(sys.argv))          # real argv (test harness passes none): program name only
    print(len(sys.argv) > 1)      # -> False here, but reflects the real invocation now
    print(maxsize > 1000000000)
    sys.setrecursionlimit(10000)  # no-op
    print("done")

main()
