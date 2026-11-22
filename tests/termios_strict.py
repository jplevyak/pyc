# Under --strict, termios.tcgetattr is refused: CPython's cc list mixes
# 1-byte bytes and ints, which pyc can only return as all ints, the
# permissive PYC_TERMIOS_CC_INT accommodation (pyc_lib/termios.py).
# tcsetattr, which takes either kind, stays available.
import os
import termios

m, s = os.openpty()
attrs = termios.tcgetattr(s)
print(attrs[3] & termios.ICANON)
