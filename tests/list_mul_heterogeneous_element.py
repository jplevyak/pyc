# issues/035: `n * [0]` then an in-place float update of one element.
# The list genuinely becomes heterogeneous (int | float), which pyc's
# general dynamic-list representation does not scope per allocation site.
# pyc compiles this with ZERO diagnostics and the binary aborts with
# "list element type mismatch"; CPython prints [1.5, 0, 0].
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
