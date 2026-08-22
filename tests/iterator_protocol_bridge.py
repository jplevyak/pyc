# issues/116: a class implementing CPython's iterator protocol
# (__iter__/__next__ raising StopIteration) must iterate correctly.
# pyc's own protocol is peek-then-fetch (__pyc_more__/__next__); the
# __pyc_iterator__ bridge in __pyc__/00_runtime.py adapts one to the
# other for any user class that defines __next__ but no __pyc_more__.


class Counter:
    def __init__(self, n):
        self.i = 0
        self.n = n

    def __iter__(self):
        return self

    def __next__(self):
        if self.i >= self.n:
            raise StopIteration(0)
        self.i = self.i + 1
        return self.i


# for loop
for x in Counter(3):
    print(x)

# comprehension, list(), and `in`
print([x * 2 for x in Counter(3)])
print(list(Counter(4)))
print(2 in Counter(3), 9 in Counter(3))

# a subclass inherits the bridge
class Sub(Counter):
    pass


print([x for x in Sub(2)])

# two instances keep independent state
a = Counter(2)
b = Counter(2)
print(a.__next__(), b.__next__(), a.__next__(), b.__next__())

# nested loops over separate iterators
out = []
for x in Counter(2):
    for y in Counter(2):
        out.append((x, y))
print(out)


# A class that already speaks pyc's protocol is left alone: it defines
# __pyc_more__, so no bridge is inserted and __next__ stays its own
# (verified by the absence of __pyc_user_next__ in the emitted C).
# It still raises StopIteration so CPython, which has no __pyc_more__,
# terminates on the same three values.
class Fast:
    def __init__(self, n):
        self.i = 0
        self.n = n

    def __iter__(self):
        return self

    def __pyc_more__(self):
        return self.i < self.n

    def __next__(self):
        if self.i >= self.n:
            raise StopIteration(0)
        self.i = self.i + 1
        return self.i


print([x for x in Fast(3)])


# a subclass that OVERRIDES __next__ is renamed too, so the override
# lands on the name the bridge it already inherits actually calls -- and
# the bridge is NOT added a second time (C3 rejects listing a base
# alongside something that derives from it).
class Doubling(Counter):
    def __next__(self):
        if self.i >= self.n:
            raise StopIteration(0)
        self.i = self.i + 1
        return self.i * 10


print([x for x in Doubling(3)])


# a base that speaks pyc's protocol natively keeps it for its subclasses
# too: FastSub supplies only __next__, and FastBase.__pyc_more__ pairs
# with it under the real name. Bridging here would give __pyc_more__ two
# unrelated candidates, which dispatch reports as ambiguous.
#
# FastBase deliberately does NOT define __next__ of its own: a subclass
# OVERRIDING an inherited method hits issues/119 (the override gets a
# second member slot, shifting every field after it while a method
# shared with the base still blind-casts to the base's layout), which
# has nothing to do with this issue and hangs.
class FastBase:
    def __init__(self, n):
        self.i = 0
        self.n = n

    def __iter__(self):
        return self

    def __pyc_more__(self):
        return self.i < self.n


class FastSub(FastBase):
    def __next__(self):
        if self.i >= self.n:
            raise StopIteration(0)
        self.i = self.i + 1
        return self.i * 100


print([x for x in FastSub(3)])


# an exception that is NOT StopIteration propagates out of the loop
class Boom(Exception):
    value = 0

    def __init__(self, value=0):
        self.value = value


class Bad:
    def __init__(self):
        self.i = 0

    def __iter__(self):
        return self

    def __next__(self):
        self.i = self.i + 1
        if self.i > 2:
            raise Boom(42)
        return self.i


try:
    for v in Bad():
        print("v", v)
except Boom as e:
    print("caught", e.value)


# ...and so does one raised from a generator body (the same missing
# post-advance check: pre-existing, independent of the bridge).
def g():
    yield 1
    yield 2
    raise Boom(7)


try:
    for v in g():
        print("g", v)
except Boom as e:
    print("caught", e.value)
