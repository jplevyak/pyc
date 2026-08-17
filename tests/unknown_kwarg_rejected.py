# ifa/issues/103: an unrecognized keyword argument must not be silently
# bound to the next positional parameter.
#
# CPython raises `TypeError: f() got an unexpected keyword argument
# 'nosuchkw'`. pyc's matcher looks the name up in the callee's
# `named_to_positional` map (ifa/if1/pattern.cc build_positional_map),
# gets null when there is no such formal, and -- rather than rejecting
# the candidate -- leaves the actual in the "not used by a named
# argument" set, where it is assigned POSITIONALLY. So `B` silently
# receives 99.
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
