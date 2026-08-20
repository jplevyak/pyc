# ifa/issues/090 / issues/025 item 4: `x in gen()`.
#
# `in` lowers to a direct __contains__ dispatch on the right operand,
# with no fallback to the iterable protocol when the method is absent
# -- and __pyc_generator__ had none, so membership on a generator could
# not resolve at all. Even `3 in gen()` over plain ints failed.
#
# INT yields only: a generator's value channel is int-typed, so a
# yielded tuple/str comes back as a reinterpreted pointer (issues/114).
def gen():
    yield 1
    yield 2
    yield 3

print(3 in gen())
print(5 in gen())
print(1 in gen())

n = 0
while n not in gen():
    n = 2
print(n)
