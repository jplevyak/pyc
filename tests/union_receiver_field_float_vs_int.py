# ifa/206: `R` splits into an int-`v` contour and a float-`v` contour, and
# `r` holds both. The read of `r.v` was emitted through ONE member's layout,
# so on the int object it read the int64's bits as a double: CPython prints
# 3, pyc printed 1.5e-323 with only the permissive int/float warning.
#
# Step 1 refused it (the layout contract now requires the representation to
# agree at the accessed slot, not only the width). Step 2 satisfies the
# demand: the two clones of `R` get their own class tags, and the read tests
# the tag and reads through the object's own layout. `+ 0.5` keeps the
# permissive int->float widening of `r.v` out of what is printed.
# tests/method_slot_registered_on_concrete_type.py is the sound shape the
# same rule must keep accepting without a tag test.
import sys


class R:
    def __init__(self, v):
        self.v = v


for i in range(2):
    r = R(3) if (len(sys.argv) + i) % 2 == 1 else R(4.5)
    print(r.v + 0.5)
