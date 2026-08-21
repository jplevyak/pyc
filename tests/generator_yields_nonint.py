# issues/114: a generator's value channel carries whatever the body
# yields, not just int. Each `yield` contributes its value's TYPE to the
# generator function's fn->ret (python_ifa_build_if1.cc's
# gen_yield_type_contribution), the wrapper hands that type to
# __pyc_generator__ as a sample, and the c_calls in
# __pyc__/09_generator.py take their result type from it. Before this,
# a yielded tuple came back as a reinterpreted pointer and printed as a
# raw address, with no diagnostic at all.

def tuples():
    yield (1, 2)
    yield (3, 4)

for x in tuples():
    print(x, len(x), x[0], x[1])

# Several yields must UNION, not overwrite each other. fn->ret is
# single-assignment-renamed, so sequential moves into it kill each
# other and only the last survives -- which reported the last tuple's
# values on every iteration. Each yield gets its own path to the reply
# so the join unions them instead.
def three():
    yield (1, 2)
    yield (3, 4)
    yield (5, 6)

for a, b in three():
    print(a, b)

def strings():
    yield "ab"
    yield "cde"

for s in strings():
    print(s, len(s))

def lists():
    yield [1, 2]
    yield [3]

for l in lists():
    print(l)

# Containment over a non-int generator (__pyc_generator__.__contains__).
print((3, 4) in tuples(), (9, 9) in tuples())

# A bare `return` inside a non-int generator contributes nothing to the
# channel. It used to move an int 0 into fn->ret, making the channel
# {int, tuple} -- an outright "matching function not found" abort.
def bare_return(n):
    yield (1, 2)
    if n > 0:
        return
    yield (3, 4)

for x in bare_return(0):
    print(x)
for x in bare_return(1):
    print(x)

# A generator whose body yields exactly ONE constant and then raises.
# FA proves the return is the constant 1 -- but a generator's C return
# value is the coroutine handle, not its reply value, so inlining that
# literal built the generator object around the address 1 and the first
# resume segfaulted (fa.cc's P_prim_reply widens it now).
class Boom(Exception):
    value = 0
    def __init__(self, value=0):
        self.value = value

def one_then_raise():
    yield 1
    raise Boom(77)

g = one_then_raise()
print(g.__next__())
try:
    g.__next__()
except Boom as exc:
    print(exc.value)
