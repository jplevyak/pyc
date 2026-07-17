# issue 011 follow-on: __pyc_assert_fail__ now raises a catchable
# AssertionError instead of print+exit, so `assert` composes with
# try/except like any other exception source.
def check(n):
    assert n > 0, "n must be positive"
    return n * 2

try:
    print(check(5))
    print(check(-1))
except AssertionError as e:
    print("caught assertion: " + str(e))
print("after")

# A function whose ENTIRE body is an unconditional raise (no other
# return path at all) -- exercises goto_exc_target's fn->ret handling
# when there is no return statement anywhere in the function to
# otherwise supply one.
def always_fails():
    raise ValueError("bad")

try:
    always_fails()
except ValueError as e:
    print("caught: " + str(e))
