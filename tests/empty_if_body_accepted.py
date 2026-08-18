# issues/106: pyc silently accepts an `if:` with NO BODY inside a
# function. CPython raises
#   IndentationError: expected an indented block after 'if' statement
# pyc parses it, drops the `if` entirely, and prints 2.
#
# Found while delta-reducing shedskin_examples/plcfrs (ifa/issues/105):
# the reduction produced a 93-line "repro" that CPython rejects outright,
# because the oracle only checked for the target error and pyc's parser
# happily accepted the malformed intermediate. Any delta reduction of
# Python for pyc must therefore validate candidates with ast.parse.
#
# Note the same shape at MODULE level IS rejected ("dparse: parse error"),
# so this is specific to a suite inside an indented block.
def f(a):
    if a == 1:
    b = 2
    return b
print(f(1))
