# issues/171 #7: `except StopIteration as e: e.value` reads the value
# through the program's one global exception slot (__pyc_exc__), where
# EVERY StopIteration meets: this handler can only ever catch `a`'s
# (value "s"), but it is typed with `b`'s int and the value-less ones
# the library and fall-through generators raise as well. The
# StopIteration CreationSets themselves are separated by value type;
# the confluence is the slot, which is not flow- or call-graph-
# sensitive. `yield from` no longer reads the slot (it asks the
# sub-generator), so only an explicit handler is affected.
def a():
    yield 1
    return "s"

def b():
    yield 2
    return 7

def c():
    yield 3

for v in b():
    pass
for v in c():
    pass

g = a()
print(next(g))
try:
    next(g)
except StopIteration as e:
    print(e.value)
