# ifa/issues/091: int/float/bool/list/tuple stored as a plain value
# and called indirectly (`factory = int; factory()`) had no real
# __new__ to dispatch to -- fixed by giving each a genuine zero-arg
# __new__ attached to its meta_type, the same registration dict/set
# already use for their real __new__ wrapper.
#
# Deliberately 5 SEPARATE variables, not one reused across types: a
# single variable holding int/float/bool/list/tuple across different
# statements merges into one shared polymorphic call contour, which
# hits a distinct, separate, already-known FA/codegen gap (mixing
# unboxed concrete return types -- e.g. float and bool -- through one
# call site needs boxing pyc doesn't do yet). Real callers (like
# defaultdict below) don't hit this: each instance's `self.factory`
# is contour-split per construction site by pyc's existing clone
# splitting, never merged across differently-typed constructions.
factory_int = int
print(factory_int())
factory_float = float
print(factory_float())
factory_bool = bool
print(factory_bool())
factory_list = list
print(factory_list())
factory_tuple = tuple
print(factory_tuple())

# The concrete real-world motivator: collections.defaultdict's
# auto-vivify path (self.d[key] = self.factory()), only exercised on
# a __getitem__ MISS -- an explicit d[k] = v never calls factory() at
# all, so this needs the `d[k] += 1` / `.append()`-on-miss shape
# specifically.
from collections import defaultdict

counts = defaultdict(int)
counts["a"] += 1
counts["a"] += 1
counts["b"] += 5
print(counts["a"], counts["b"], counts["c"])

groups = defaultdict(list)
groups["x"].append(1)
groups["x"].append(2)
print(groups["x"], groups["y"])

# Must not regress: the direct-call-site fast paths (matched earlier,
# in build_if1.cc, before any generic dispatch is considered) and
# isinstance()'s use of these same class values as type descriptors
# (the exact thing the reverted 2026-08-10 load-site-substitution
# attempt broke).
print(int(), int(5), int("42"), int("ff", 16))
print(float(), float(3.5))
print(bool())
print(list(), list([1, 2, 3]))
print(tuple(), tuple([1, 2, 3]))
print(isinstance(5, int), isinstance("x", int))
print(isinstance([1, 2], list), isinstance((1, 2), tuple))
print(isinstance(True, bool))
