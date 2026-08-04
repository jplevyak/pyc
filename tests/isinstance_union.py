# ifa/issues/025 Case 2 / ifa/issues/030 cross-ref: isinstance(x, C)
# against a union-typed x (e.g. iterating a heterogeneous list) used
# to always return False, regardless of the real runtime object.
# Root cause: isinstance(obj, cls) routed through __pyc__/05_builtins
# .py's shared Python-level wrapper -- once two different classes were
# checked anywhere in the program, FA generalized both calls into one
# shared clone taking a runtime class value, and that shared clone got
# mis-constant-folded to a hardcoded `return 0`. Fixed by recognizing
# a direct isinstance(obj, cls) call in the frontend and building the
# same raw sym_primitive/"isinstance" send every other isinstance
# lowering in this codebase already uses (is-None, match/case,
# except-clause, yield-from's StopIteration check) -- each call site
# gets its own genuinely monomorphic send, nothing left to share.

class Animal:
    pass

class Dog(Animal):
    pass

class Cat(Animal):
    pass

def describe(a):
    if isinstance(a, Dog):
        return "dog"
    elif isinstance(a, Cat):
        return "cat"
    return "unknown"

animals = [Dog(), Cat()]
for a in animals:
    print(describe(a))

# Two direct, non-union checks against different classes in the same
# function -- confirms the fix doesn't merely mask the bug for the
# monomorphic case while still relying on the (now-removed) shared
# clone elsewhere.
d = Dog()
c = Cat()
print(isinstance(d, Dog), isinstance(d, Cat))
print(isinstance(c, Dog), isinstance(c, Cat))
