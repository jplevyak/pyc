# @total_ordering is lowered on a module-level class only.
from functools import total_ordering


def make():
    @total_ordering
    class C:
        def __lt__(self, other):
            return True
    return C


print(make())
