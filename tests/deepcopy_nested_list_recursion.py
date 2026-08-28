# ifa/issues/105: `copy.deepcopy` of a NESTED list, consumed by a
# self-recursive function, degenerates to a {int64, list} union.
#
# This is shedskin_examples/linalg's compile failure, reduced from 239
# lines to 10 by delta-debugging (statement-level, oracle = ast.parse +
# CPython exit 0 + pyc's refusal). CPython prints 4; pyc refuses:
#
#   fail: a variable holding {int64, list, list} has no representation:
#   '__add__' resolved to the CONTAINER method, whose receiver may be a
#   scalar. pyc does not box, so a {container, scalar} union cannot be
#   represented (issues/018)
#
# The mechanism is `list.__deepcopy__` in __pyc__/04_sequence.py:
#
#     def __deepcopy__(self):
#       r = []
#       for k in range(len(self)):
#         r.append(self[k].__deepcopy__())
#       return r
#
# -- 105's "local accumulator in a shared generic method", one contour
# for every receiver. Copying a list-of-lists enters it TWICE: at the
# outer level `self[k]` is a `list`, at the inner level an `int64`, and
# both append into the same `r`. So the copy's element type comes back
# as the union of the nesting levels, and `M[0][0] + ...` then resolves
# `__add__` on a receiver that may be either.
#
# CAUSAL, not correlational -- unlike the earlier 105/104 theories.
# Substituting a hand-written copy for `copy.deepcopy` in the REAL
# linalg (`M1=[row[:] for row in M]`, one line) takes it from 62
# warnings + this refusal to a clean rc=0 compile.
#
# Three controls, each of which compiles cleanly, pin the ingredients:
#
#   - `M1 = M[:]` instead of `copy.deepcopy(M)`   -> clean (deepcopy)
#   - a FLAT list (`M[0]`, not `M[0][0]`)         -> clean (nesting)
#   - `return head_sum(M1)`, dropping the `+`     -> clean (the use)
#
# The first two are the cause; the third only decides whether the union
# is diagnosed here or escapes into broken C further on (with `*` in
# place of `+` this same program emits `no matching function for call to
# '_CG_list_mult_internal'` instead of refusing).
import copy

def head_sum(M):
    if len(M) == 1:
        return M[0][0]
    M1 = copy.deepcopy(M)
    M1.pop(0)
    return M[0][0] + head_sum(M1)

print(head_sum([[1, 2], [3, 4]]))
