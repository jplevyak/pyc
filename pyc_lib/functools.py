class _empty_initializer:
    pass

def reduce(function, iterable, initializer=_empty_initializer):
    it = iter(iterable)
    if initializer is _empty_initializer:
        try:
            value = next(it)
        except StopIteration:
            raise TypeError("reduce() of empty sequence with no initial value")
    else:
        value = initializer
    for element in it:
        value = function(value, element)
    return value
