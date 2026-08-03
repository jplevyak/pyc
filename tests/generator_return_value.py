# issues/014: `return value` inside a generator (-> StopIteration.value
# in real Python) used to be silently discarded -- explicit `return X`
# was parsed and lowered, but codegen (cg.cc's is_generator handling)
# always emitted a bare `co_return;`/`co_return;`-equivalent, ignoring
# the real value entirely, and __pyc_generator__.__next__()/.send()
# never raised on exhaustion at all (returned stale data instead).
def gen():
    yield 1
    yield 2
    return 42

g1 = gen()
print(g1.__next__())
print(g1.__next__())
try:
    g1.__next__()
except StopIteration as exc1:
    print(exc1.value)

# Same, but the generator's own control flow never falls through on
# its own (an unconditional `while True:`, issues/014's other recent
# fix) -- exercises the interaction between the two: the return value
# must still reach StopIteration.value when the exit is a conditional
# `return` inside an otherwise-infinite loop.
def counter_with_limit():
    i = 0
    while True:
        if i >= 3:
            return i * 100
        yield i
        i += 1

g2 = counter_with_limit()
print(g2.__next__())
print(g2.__next__())
print(g2.__next__())
try:
    g2.__next__()
except StopIteration as exc2:
    print(exc2.value)

# .send()-driven exhaustion also raises with the real return value.
def echo_then_stop():
    x = yield 1
    return x + 1000

g3 = echo_then_stop()
print(g3.__next__())
try:
    g3.send(5)
except StopIteration as exc3:
    print(exc3.value)
