# issues/182: a keyword naming a formal that a positional already bound.
# CPython: TypeError: f() got multiple values for argument 'a'
def f(a, **kw):
    return a + len(kw)

print(f(1, a=3))
