# issues/171 #12: a recursive call inside a decorated function reaches the
# DECORATED binding, as in CPython (it used to call the undecorated function,
# so a counting or memoising decorator never saw the recursion). The nested
# case also needs the def closure-converted before it is decorated.
calls = [0]

def counted(f):
    def wrapper(n):
        calls[0] += 1
        return f(n)
    return wrapper

@counted
def fact(n):
    if n <= 1:
        return 1
    return n * fact(n - 1)

print(fact(5))
print(calls[0])

def memo(f):
    cache = {}
    def wrapper(n):
        if n in cache:
            return cache[n]
        r = f(n)
        cache[n] = r
        return r
    return wrapper

@memo
def fib(n):
    if n < 2:
        return n
    return fib(n - 1) + fib(n - 2)

print(fib(25))

def outer(k):
    @counted
    def down(n):
        if n == 0:
            return k
        return down(n - 1)
    return down(3)

calls[0] = 0
print(outer(9))
print(calls[0])
