# issues/035: `n * [0]` then an in-place float update of one element.
# The list genuinely becomes heterogeneous (int | float), which pyc's
# general dynamic-list representation does not scope per allocation site.
# Since 2026-08-16 pyc widens the element union to float (issue 025's
# numeric coercion, now applied to container elements) and the program
# RUNS -- it prints [1.5, 0.0, 0.0] where CPython prints [1.5, 0, 0].
# shedskin diverges identically, by the same rule; the remaining gap is
# the repr, not the arithmetic. Before that it compiled with zero
# diagnostics and aborted with "list element type mismatch".
#
# Check files describe the CORRECT behaviour; the .known_issue tag keeps
# it out of the failure count. Delete the tag when fixed.
def make_heterogeneous_list():
    n = 3
    x = n * [0]
    i = 0
    x[i] += 1.5
    return x

print(make_heterogeneous_list())
