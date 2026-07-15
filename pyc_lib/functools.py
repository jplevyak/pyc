# pyc shim for functools. reduce uses the default-None + is-None
# narrowing pattern (see __pyc__/05_builtins.py min/max) instead of
# CPython's private sentinel class -- a class-object default is
# untypable for pyc today. reduce(f, seq, None) as an EXPLICIT
# initializer is therefore indistinguishable from the 2-arg form
# (nobody in the corpus does that).

def reduce(function, iterable, initializer=None):
    it = iter(iterable)
    if initializer is None:
        value = next(it)
    else:
        value = initializer
    for element in it:
        value = function(value, element)
    return value
