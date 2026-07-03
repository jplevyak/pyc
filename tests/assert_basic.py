assert True
assert 1 == 1
assert 1 == 1, "should not print"
print("ok")


def check(x):
    assert x > 0, "x must be positive"
    return x * 2


print(check(5))


def side_effect():
    print("side effect ran")
    return "computed message"


assert True, side_effect()
print("done")
