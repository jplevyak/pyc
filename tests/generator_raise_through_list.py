# a raise inside a generator reaches the handler around list(), tuple(), sorted() or a for loop consuming it (issues/207).
def g(n):
    yield 1
    if n > 0:
        raise ValueError("boom")
    yield 2
try:
    print(list(g(1)))
except ValueError as e:
    print("caught list", e)
try:
    for x in g(1):
        print("got", x)
except ValueError as e:
    print("caught for", e)
try:
    print(sorted(g(1)))
except ValueError as e:
    print("caught sorted", e)
try:
    print(tuple(g(1)))
except ValueError as e:
    print("caught tuple", e)
print(list(g(0)), tuple(g(0)))
