def gen():
    yield 1
    yield 2

for v in gen():
    print(v)

def squares(n):
    for i in range(n):
        yield i * i

for v in squares(5):
    print(v)

def counter(start, stop):
    i = start
    while i < stop:
        yield i
        i += 1

for v in counter(3, 7):
    print(v)

def codes(s):
    for c in s:
        yield ord(c)

for v in codes("ab"):
    print(v)

def echoer(vals):
    for v in vals:
        yield v

for v in echoer([10, 20, 30]):
    print(v)

# Interleaved manual next() calls (not driven by a for loop), plus
# mixing direct .__next__() calls with a for loop on the same
# generator afterward (issues/014 item 3).
def counting_up():
    for i in range(5):
        yield i * i

g = gen()
print(g.__next__())
print(g.__next__())

c = counting_up()
print(c.__next__())
print(c.__next__())
for v in c:
    print(v)

# .send(): delivers a value into the generator's paused `x = yield`
# expression.
def running_total():
    total = 0
    for i in range(4):
        x = yield total
        total += x

rt = running_total()
print(rt.__next__())
print(rt.send(10))
print(rt.send(5))
print(rt.send(1))
