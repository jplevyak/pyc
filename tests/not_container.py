# `not <container>` for builtin containers. Containers don't derive from
# `object` (builtin classes are exempt from the implicit object base) and
# `__pyc_any_type__` -- their actual root -- had no `__not__`, so
# `not <list/tuple/dict/set/str>` dispatched to nothing ("matching
# function not found" / "getter not resolved") for EVERY container, empty
# or not. `if not somelist:` is everyday Python; no test covered it.
# Fixed by adding `__not__` to `__pyc_any_type__`.

print(not [1, 2, 3])     # False
print(not (1, 2, 3))     # False
print(not {1: 2})        # False
print(not {1, 2})        # False
print(not "abc")         # False
print(not [])            # True
print(not [0])           # False

# inside a function, over a list built from a range (the chess line-314
# shape: `if not <list expression>:`)
def evens_present(n):
    evens = [i for i in range(n) if i % 2 == 0]
    if not evens:
        return "none"
    return "some"

print(evens_present(0))   # none
print(evens_present(5))   # some
