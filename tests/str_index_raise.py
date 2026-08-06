s = "2....64.1"
n = 0
for digit in "123456789":
    try:
        n += s.index(digit)
    except ValueError:
        n -= 1
print(n)

try:
    s.index("x")
    print("no exception")
except ValueError:
    print("caught")
