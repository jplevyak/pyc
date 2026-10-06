# CPython: values of unrelated types are never equal. int/float `==`
# applied the numeric primitive to any operand ("illegal primitive argument
# type" on a str), str `==` passed the operand to _CG_str_eq as a str (a
# run-time "C call argument type mismatch" abort), and bool `==` returned
# the operand itself (`True == 5` printed 5).
a = 1
b = "a"
c = 2.5
t = (1, 2)
print(a == b, b == a, c == b, b == c, a != b, b != a, c != b)
print(t == b, b == t, a == t)
print(a == c, a == 1, c == 2.5, b == "a", a == 1.0)
print(True == a, True == 5, False == 0, True == 1.0, False == 0.5)
print(True == b, True != b, True == True, False == True, True != False)
print(a == True, c == False)
