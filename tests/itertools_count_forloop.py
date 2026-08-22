# issues/116: pyc's for-loop protocol is peek-then-fetch (__iter__ /
# __pyc_more__ / __next__), not CPython's fetch-until-StopIteration, and
# `object.__pyc_more__` returns False. So a class defining only the
# standard Python __iter__/__next__ -- as pyc_lib/itertools.py's `count`
# did -- iterated ZERO times in a `for`, silently and with no
# diagnostic. `for j in count(...)` printed nothing at all.
#
# The general protocol gap is still open (see the issue); this covers
# `count`, which now implements __pyc_more__ directly.

from itertools import count

for j in count(0, 1):
    if j >= 4:
        break
    print(j)

# Non-unit and negative steps.
out = []
for j in count(10, -3):
    if j <= 0:
        break
    out.append(j)
print(out)

# next() on the same object still works (the pre-existing use).
c = count(5, 2)
for _ in range(3):
    print(next(c))

# A count driving a loop inside a generator -- sunfish's shape.
def rays(start, step):
    for j in count(start, step):
        if j < 0 or j >= 6:
            break
        yield (start, j)

print([t for t in rays(2, 1)])
