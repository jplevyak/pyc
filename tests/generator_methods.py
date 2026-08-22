# issues/115: a `yield` inside a class method makes a generator like any
# other. It never did: __pyc_generator__ was only ever constructed by
# the wrapper build_if1_pyda's PY_funcdef case synthesizes for a plain
# def, and that path was gated on the def NOT being a method -- so a
# method call returned the bare coroutine handle, which has no
# __iter__/__pyc_more__/__next__, and every generator method ever
# written failed with "unresolved call '__iter__'". Not type-specific:
# the int case below failed too.

class Counter:
    def __init__(self, base):
        self.base = base
    def nums(self):
        yield self.base
        yield self.base + 1

for m in Counter(7).nums():
    print(m)

# Yielding a non-int through a method (issues/114's channel typing, now
# reachable from a method too).
class Moves:
    def __init__(self, a):
        self.a = a
    def pairs(self):
        yield (self.a, 1)
        yield (self.a, 2)

p = Moves(7)
for t in p.pairs():
    print(t, len(t), t[0], t[1])

print((7, 1) in p.pairs(), (9, 9) in p.pairs())

# The sunfish shape: a None-seeded membership loop over a method
# generator.
mv = None
while mv not in p.pairs():
    mv = (7, 1)
print("found", mv)

# Arguments besides self forward through the wrapper.
class Ranged:
    def __init__(self, base):
        self.base = base
    def upto(self, n, step):
        i = 0
        while i < n:
            yield (self.base + i, step)
            i = i + step

for t in Ranged(10).upto(5, 2):
    print(t)

# A subclass inherits the wrapper, not the raw coroutine body: the
# member's alias is what gen_class_pyda copies into the subclass.
class Sub(Ranged):
    pass

for t in Sub(100).upto(3, 1):
    print(t)

# A same-named generator method on an unrelated class must not capture
# the dispatch: the wrapper's `self` formal is specialized to its own
# class, not left open.
class Other:
    def upto(self, n, step):
        yield "other"

for s in Other().upto(1, 1):
    print(s)

# yield from between two methods of the same object.
class Delegating:
    def inner(self):
        yield 1
        yield 2
    def outer(self):
        yield from self.inner()
        yield 3

for v in Delegating().outer():
    print(v)

# .send() into a method generator.
class Echo:
    def echo(self):
        x = yield 1
        yield x + 10

e = Echo().echo()
print(e.__next__())
print(e.send(5))
