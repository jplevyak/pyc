# list.sort() with defaulted args on the CLONED builtin list class
# (regression: default-arg globals were initialized in the class-body
# ___init___ closure, which is only ever CALLED for Type_RECORD
# classes -- core non-record builtins like list never ran it, so a
# bare `a.sort()` silently no-opped while `a.sort(None, False)`
# worked; see gen_fun_pyda's literal-default fast path).
# NOTE: TWO different default-subset shapes of the same method in one
# program (e.g. a.sort() plus b.sort(reverse=True)) still trip the
# issue 040 cross-contour union at runtime -- this test deliberately
# uses one shape; list.sort(key=...) is exercised on its own in the
# corpus (circle.py).
a = [3, 1, 2]
a.sort()
print(a)
d = [3, 1, 2]
d.reverse()
print(d)
print(sorted([5, 2, 9]))

# list(str) via str.__pyc_tolist__
print(list("abc"))

# id(): identity comparison idiom
e = [1, 2]
f = e
g = [1, 2]
print(id(e) == id(f))
print(id(e) == id(g))

# hash(): self-consistent within a run
print(hash("hello") == hash("hello"))
print(hash("hello") == hash("world"))
print(hash(42))

# Standard exception classes exist and format
print(str(ValueError("bad value")))
print(str(IndexError("oob")))
