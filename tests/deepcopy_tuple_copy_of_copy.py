# issues/112: deep-copying a tuple TWICE (copy-of-copy) leaves `self[k]`
# unresolved inside tuple.__deepcopy__.
#
# It blocked the PYC_MAKESEQ flip: with `tuple(iterable)` a real tuple and
# `class tuple` given a __deepcopy__, a copy of a copy left `self[k]`
# unresolved. Both are the default now (2026-09-28) and it passes; the
# generated __deepcopy__ constructs its result rather than copying and
# overwriting (python_ifa_main.cc, inject_tuple_methods).
#
# Not tagged: it passes as shipped. It is here to pin the shape.
import copy
class T:
    def __init__(self, args=None):
        self.args = args
        self.value = -1
def count(node):
    n = 1
    if node.args:
        for k in range(len(node.args)):
            n += count(node.args[k])
    return n
leaf = T(None)
tree = T(tuple([leaf]))
print(count(tree))
c1 = copy.deepcopy(tree)
c2 = copy.deepcopy(c1)
c2.args[0].value = 77
print(tree.args[0].value, c1.args[0].value, c2.args[0].value)
