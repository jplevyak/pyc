# ifa/issues/047 (closed): iterating tuples of different arity
# and/or element type in one program used to share ONE
# __tuple_iter__ CreationSet. The class-body ___init___ installed
# __pyc_more__/__next__ into the shared PROTOTYPE's method-pointer
# slots, so every iterator instance got whichever clone was
# installed last -- wrong-arity length checks and out-of-bounds
# tuple::__getitem__, segfaulting. Fixed (incidentally, by the
# broader per-creation-site CS-splitting work) before this test was
# added: each for-loop's tuple.__iter__() call now resolves to its
# own CreationSet, whose __pyc_more__/__next__ clones are called
# directly rather than through the shared prototype slot.

# three different arities
for x in (1, 2):
    print(x)
for y in (30, 40, 50):
    print(y)
for z in (100, 200, 300, 400):
    print(z)

# same arity, different element types
for s in ("a", "b"):
    print(s)
for f in (1.5, 2.5):
    print(f)

# a shared function iterating differently-shaped tuples across calls
def show(t):
    for v in t:
        print(v)

show((7, 8, 9))
show((11, 12))
show(("x", "y", "z"))
