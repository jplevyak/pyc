# and/or in conditional context
def test_cond(a, b, c):
    if a and b and c:
        print('and true')
    else:
        print('and false')
    if a or b or c:
        print('or true')
    else:
        print('or false')

test_cond(True, True, True)
print('and true *')
print('or true *')
test_cond(False, False, False)
print('and false *')
print('or false *')
test_cond(True, False, True)
print('and false *')
print('or true *')
test_cond(False, True, False)
print('and false *')
print('or true *')
test_cond(True, True, False)
test_cond(False, False, True)
print('and false *')
print('or true *')
test_cond(True, False, False)
test_cond(False, True, True)
print('and false *')
print('or true *')

# and/or/not as value expressions
print(1 and 2)
print(1 and 45)
print(1 and 0)
print(0 and 2)
print(1.0 and 0.0)
print(0.0 and 2.0)
print(2 or 3)
print(2 or 0)
print(2.0 or 3.0)
print(2.0 or 0.0)
print(not 0)
print(not False)
print(not True)

class A:
    x = 2

a = A()
print((None or a).x)
b = []
c = [1, 2, 3]
print(b or c)

# and/or with False literal values
print(False and 0)
print(False or 3)
