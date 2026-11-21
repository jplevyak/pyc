# a lambda anywhere inside a comprehension; its nesting depth counted the
# comprehension's inline scope and crashed flow analysis (issues/207).
print([(lambda y: y + 1)(x) for x in [1, 2]])
print([x for x in map(lambda y: y + 1, [1, 2])])
print([x for x in [1, 2, 3] if (lambda y: y > 1)(x)])
def f(k):
    return [x for x in map(lambda y: y + k, [1, 2])]
print(f(10))
print({x: (lambda z: z * 2)(x) for x in [1, 2]}, sorted({(lambda z: -z)(x) for x in [1, 2]}), sum((lambda z: z)(x) for x in [3, 4]))
print([[(lambda w: w + i)(j) for j in range(2)] for i in range(2)])
def g(n):
    h = lambda a: a * n
    return [h(x) for x in range(3)] + [(lambda b: b + n)(x) for x in range(2)]
print(g(5))
