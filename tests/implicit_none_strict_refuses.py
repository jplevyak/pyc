# issues/171: under --strict pyc keeps CPython's implicit None. It used to
# turn ifa_no_implicit_none ON, which dropped the fall-off None arm, and FA
# then folded the return to the one explicit value: f([0]) printed 1 where
# CPython prints None, silently. A {int, None} return has no representation
# (issues/048), so strict now refuses instead.
def f(x):
    for k in x:
        if k:
            return 1

print(f([0]))
print(f([1]))
