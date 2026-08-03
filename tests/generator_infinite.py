# issues/014: a generator whose body never falls through on its own
# (`while True:` with no `break`/`return` anywhere) used to fail to
# compile -- FA never visits the function's fall-off-the-end reply
# node (unreachable given the loop has no exit edge at all), so it
# infers the coroutine body's return type as bottom/NOTYPE, breaking
# the synthesized __pyc_generator__-constructing wrapper downstream.
def counter():
    i = 0
    while True:
        i += 1
        yield i

c = counter()
for _ in range(5):
    print(c.__next__())

# Same shape, but driven by .send() into a paused `x = yield total`
# expression -- exercises the infinite-loop fix together with the
# existing .send() value-delivery mechanism (issues/014 item 3).
def echo_forever():
    total = 0
    while True:
        x = yield total
        total += x

e = echo_forever()
print(e.__next__())
print(e.send(10))
print(e.send(5))
print(e.send(1))
