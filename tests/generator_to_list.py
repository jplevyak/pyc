# list(gen()): the list() intercept needs __pyc_tolist__
# on the generator class.
def g(n):
    for i in range(n):
        yield i * i

def count(n):
    if n > 0:
        yield n
        for x in count(n - 1):
            yield x

print(list(g(4)))
print(list(count(3)))
print(sum(list(g(5))))
