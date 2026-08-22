# issues/114 repro B -- FIXED 2026-08-21. Was the recorded blocker.
#
# Two yields put two values on the generator's channel. This used to
# report the SECOND tuple on every iteration:
#
#   CPython:      2 1 2        was:  2 3 4
#                 2 3 4              2 3 4
#
# The diagnosis written at the time -- "constant folding across a
# multi-CreationSet channel" -- was wrong. There was only ever ONE
# CreationSet: fn->ret is single-assignment-renamed, so the two yields'
# moves killed each other and the reply saw only the last. Each yield
# now reaches the reply on its own path, so the join unions them.
#
# The control below was the misleading part. It compares correctly and
# always did, but not for the reason assumed: a return and a reply
# union CreationSets identically. What differs is that two `return`s
# are already on separate paths and two `yield`s were not.
#
# Covered by tests/generator_yields_nonint.py.


def gen():
    yield (1, 2)
    yield (3, 4)


for x in gen():
    print(len(x), x[0], x[1])


def f(b):
    if b:
        return (1, 2)
    return (3, 4)


t = (1, 2)
print(f(1) == t, f(0) == t)   # control: True False, correct throughout
