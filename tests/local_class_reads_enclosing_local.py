# issues/169: a method of a class defined inside a function reads that
# function's local. pyc aborts in the analysis (unique_AVar: Assertion
# `es' failed) -- the method's contour has no display slot for the
# enclosing function.
def inner(x):
    class I:
        def g(self):
            return x
    return I().g()
print(inner(1))
