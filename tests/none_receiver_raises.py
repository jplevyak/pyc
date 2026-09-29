# ifa/issues/165: a None that reaches an operation None does not support
# must raise, as CPython does (TypeError here), not be read as the zero
# value. `a[1]` is the None the [None] * n preallocation left behind; the
# element type is {None, str}.
#
# Before the fix pyc printed "xy": dispatch drops None from the receiver
# (None has no __add__), so str.__add__'s `self` was the constant "x", the
# call was inlined and the receiver read deleted. Codegen now checks the
# receiver at the call site (nil_receiver_rval), the inliner leaves such a
# site alone, and DCE keeps the receiver live. pyc reports it as an
# uncaught exception (exit 1); it is not yet catchable by `except`.
def f(n):
    a = [None] * n
    a[0] = "x"
    return a[1] + "y"

print("before")
print(f(3))
print("not reached")
