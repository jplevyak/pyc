# str/repr/print of a float is CPython's repr: the SHORTEST decimal that
# round-trips, fixed-point for exponents in [-4, 16) and scientific
# otherwise, ".0" on whole numbers. pyc printed %.17g (0.1 ->
# 0.10000000000000001).
print(0.1)
print(str(0.2), repr(0.3))
print(1 / 3, 2.0, -2.5, 100.0)
print(1e16, 1e15, 123456789012345678.0)
print(0.0001, 0.00001, 1.5e-7, 1e22)
print(-0.0, float("inf"), -float("inf"))
print([0.1, 2.0], (1.5,))
print("%s" % 0.7, str(0.7) + "!")
