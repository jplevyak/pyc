# Locks in "assert False correctly aborts" -- exit code is
# necessarily nonzero, which the test harness always treats as a
# failure unless .check_fail is present (see test_pyc's EXEC-phase
# handling), so this needs that marker even though the exec.check
# content is verified correct on both backends.
print("before")
assert 1 == 2, "one is not two"
print("after")
