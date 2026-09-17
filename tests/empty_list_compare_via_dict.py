def f(xs):
    d = {}
    p = []
    for x in xs:
        d[0] = [x]
        p = d[0]
    return p == []
print(f([3.0]))
