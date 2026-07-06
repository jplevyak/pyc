# Regression: the pyc `time` stdlib shim (pyc_lib/time.py). time.time()
# is nondeterministic, so assert only deterministic properties: it
# returns a nonnegative float and is monotonic within a run; sleep is
# a no-op returning None. (issue 025 bucket C module subsystem.)
import time
from time import time as now

def main():
    t0 = time.time()
    total = 0
    for i in range(1000):
        total += i
    t1 = now()
    print(total)
    print(t0 >= 0.0)
    print(t1 >= t0)
    print(time.sleep(0) is None)

main()
