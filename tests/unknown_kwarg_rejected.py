# ifa/issues/103: an unrecognized keyword argument must not be silently
# bound to the next positional parameter.
#
# CPython raises `TypeError: f() got an unexpected keyword argument
# 'nosuchkw'`.
#
# BEFORE PYC_KWSTRICT (2026-08-16) pyc printed `[1, 2]` then `False`: the
# named actual kept its positional slot, the identity position map sent
# slot 3 to formal 3, and `default_wrapper` assigned 99 to `B`.
#
# NOW pyc rejects the match and warns at the call site. That is an
# improvement -- the misbinding is gone and there is a diagnostic -- but
# it is still not right: the report is the generic "illegal call argument
# type" rather than CPython's wording, it is a WARNING not an error, and
# the unmatched call is then elided, so the program runs and prints
# nothing. Hence still `.known_issue`.
#
# This is how shedskin_examples/life dies at runtime: it calls
# `itertools.product((0,1), repeat=rows*columns)`, pyc's product has no
# `repeat` parameter, so `repeat`'s value binds to `B` and the body then
# does `for b in B:` over an INT. That has no `__iter__` candidate, FA
# types the result bottom, and codegen emits
# `assert(!"runtime error: matching function not found")` stubs that
# abort when the loop is entered.
def f(A, B=None, C=None):
    print(A)
    print(B is None)

f([1, 2], nosuchkw=99)
