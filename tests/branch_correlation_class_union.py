# ifa/issues/025: branch-correlation narrowing over a union of USER
# CLASSES.
#
# `a` is Dog|Cat via a phi merge. The second `if flag:` correlates
# perfectly with the first -- SSU preserves flag's identity through
# the merge -- so `a` is provably Dog in the true arm and Cat in the
# false arm. IFA does not recognise that correlation: only
# isinstance / `is None` / `is not None` are discriminators today.
#
# The consequence is DIAGNOSTIC, not behavioural. Both arms dispatch
# correctly at runtime through the classtag, so the output below is
# right on every path -- but compiling it emits two spurious
# "illegal call argument type" warnings, one per arm.
#
# This is the class-typed analogue of the issue's Case 1 that the doc
# asked for. Unlike Cases 1-3, it is NOT blocked by issues/018: a
# class union has a coherent runtime representation (a classtag-headed
# pointer), which is exactly why it runs.
#
# The .check file records the correct answer -- no warnings -- so this
# flips to PASS by itself when correlation lands.


class Dog:
    def bark(self):
        return "woof"


class Cat:
    def meow(self):
        return "meow"


import sys

# Runtime-varying, so nothing folds the branches away. With no
# arguments n == 1, giving [True, False] -- both arms run.
n = len(sys.argv)
for flag in [n > 0, n > 99]:
    if flag:
        a = Dog()
    else:
        a = Cat()
    # a is Dog|Cat here
    if flag:
        print(a.bark())
    else:
        print(a.meow())
