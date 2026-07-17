# Locks in "assert False correctly raises" -- issue 011 upgraded
# __pyc_assert_fail__ to raise a real AssertionError instead of
# print+exit; uncaught here, it propagates to __pyc_unhandled_exception__
# (the "Unhandled exception: ..." message in exec.check). Exit code
# is necessarily nonzero, which the test harness always treats as a
# failure unless .check_fail is present (see test_pyc's EXEC-phase
# handling), so this needs that marker -- .check_fail also means the
# exec.check content isn't strictly diffed, just kept accurate for
# documentation.
print("before")
assert 1 == 2, "one is not two"
print("after")
