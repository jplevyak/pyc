# An unseeded `random` must still be random. pyc's MT19937 started from an
# all-zero state unless seed() was called, so random() returned 0.0 forever
# (minilight's path tracer then never ended a path). It now seeds from OS
# entropy on first use, as CPython seeds on import; a seeded stream is
# CPython's, bit for bit.
import random

xs = [random.random() for i in range(100)]
ok = True
for x in xs:
    if x < 0.0 or x >= 1.0:
        ok = False
print(ok, len(set(xs)) > 90)
random.seed(5)
print(random.random(), random.randrange(10, 100))
