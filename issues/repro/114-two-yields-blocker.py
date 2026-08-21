# issues/114 repro B -- THE BLOCKER. This is why the branch is not
# merged.
#
# Two yields put TWO CreationSets on the generator's value channel.
# Indexing then folds to compile-time constants drawn from one of
# them, so every iteration reports the SECOND tuple:
#
#   CPython:      2 1 2        this branch:  2 3 4
#                 2 3 4                      2 3 4
#
# Plausible, confident, wrong. On main the same program aborts loudly
# ("primitive operand type mismatch"), so the branch makes the failure
# mode WORSE -- a loud abort becomes a silent wrong answer. That, and
# only that, is what blocks merging.
#
# The control below is the key datum: the same two-CreationSet union
# reaching a variable through an ordinary function return compares
# correctly. So the folding is not wrong about unions in general --
# only about one arriving through the synthesised CreationSet of an
# opaque __pyc_c_call__, which is the shape this fix creates.


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
print(f(1) == t, f(0) == t)   # control: True False, correct on both
