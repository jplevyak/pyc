# A base from another module may define an ordering operation the lowering
# cannot see, which would change the root.
from functools import total_ordering
import collections


@total_ordering
class C(collections.OrderedDict):
    def __lt__(self, other):
        return True


print(C() > C())
