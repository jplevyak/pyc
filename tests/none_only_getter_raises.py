# ifa/102: an attribute read on a receiver that can only be None is
# CPython's AttributeError when it runs. `get`'s None-only contour reads
# `o.v` with nothing but None to read it from. Codegen used to write
# `assert(!"runtime error: getter not resolved")` there; it now emits the
# None check every nullable read has (nil_period_receiver, ifa/165), which
# reports CPython's message. Like that check, it exits rather than raising
# a catchable exception (165 residual 1).
class A:
    def __init__(self):
        self.v = 1

def get(o):
    return o.v

print(get(A()))
print(get(None))
print("not reached")
