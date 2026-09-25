# issues/106: an `if:` with no indented body inside a function must be a
# parse error, as it is at module level. CPython raises
#   IndentationError: expected an indented block after 'if' statement
# pyc used to accept it, drop the `if`, and print 2.
#
# The reported line is end of file, not the `if`: the check that rejects
# it is a speculative action on the enclosing compound statement, which
# DParser runs only when it reduces that statement.
def f(a):
    if a == 1:
    b = 2
    return b
print(f(1))
