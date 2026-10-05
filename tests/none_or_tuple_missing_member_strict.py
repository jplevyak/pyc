# A member no class of a {None, tuple} receiver has is a compile error, as
# it is for a plain tuple. FA used to skip the report because the receiver
# was None-only when the walk first reached the read (`x = None` arrives
# before `x = (1, 2)`), and that transient gate was never released: the
# default mode compiled it to a "getter not resolved" abort, and --strict
# crashed the compiler (cg.cc, the P_prim_period getter, on the untyped
# result). CPython raises AttributeError on both arms.
import random
x = None
if random.random() > 2.0:
    x = None
else:
    x = (1, 2)
x.append(3)
print(x)
