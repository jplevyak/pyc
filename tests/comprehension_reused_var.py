# Regression: reusing the same loop-variable name across multiple
# comprehensions in one scope. Each comprehension has its own scope
# in Python 3, so the target must not leak to the enclosing/module
# scope (which previously made a second same-named comprehension
# fail with "'i' redefined as local"). See issue 025 / bucket B.
def main():
    a = [i * i for i in range(4)]
    b = [i + 1 for i in range(4)]
    c = {i: i * 10 for i in range(3)}
    d = {i for i in range(3)}
    print(a)
    print(b)
    print(c[2])
    print(len(d))

main()
