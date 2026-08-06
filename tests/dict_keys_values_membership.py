# issues (project-level, see repo issues/ index): `x in d.keys()` /
# `x in d.values()` / `(k, v) in d.items()` were completely unresolvable
# -- `in` unconditionally dispatches to the right operand's
# __contains__ (python_ifa_build_if1.cc), and __pyc__/07_dict.py's
# __dict_iter__/__dict_items_iter__ (what .keys()/.values()/.items()
# return) never defined one. Not an imprecision bug: the method simply
# didn't exist, so the dispatch could never resolve, degrading straight
# to "no type" for ANY use of this ordinary idiom. Found via
# shedskin_examples/webserver/webserver.py's `s in self.mapSocks.keys()`.
d = {"a": 1, "b": 2, "c": 3}

print("a" in d.keys())
print("z" in d.keys())
print(1 in d.values())
print(9 in d.values())
print(("b", 2) in d.items())
print(("b", 3) in d.items())

empty = {}
print("a" in empty.keys())
print(1 in empty.values())
print(("a", 1) in empty.items())
