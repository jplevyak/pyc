# issue 025 "has no type" bucket: dict(iterable_of_pairs) had no
# dispatch (dict.__init__ only accepts zero args) -- same shape as
# set(iterable) (tests/set_from_iterable.py), needed once genexpr
# support (tests/genexpr_basic.py) started producing real programs
# that build dicts this way (shedskin's adatron.py: `dict(((x, 0.0)
# for x in AMINOACIDS))`).
#
# Kept to a single value type (str) throughout: `dict`'s _keys/_vals
# storage is one generic class program-wide, and mixing value types
# across dict() calls in one program hits a pre-existing, unrelated
# FA limitation ("expression has mixed basic types") -- reproduces
# identically with plain dict literals, nothing to do with this fix.
d = dict([(1, "a"), (2, "b"), (3, "c")])
print(len(d))
print(d[2])

# from a genexpr, and from an empty iterable.
t = dict(((x, str(x * x)) for x in [1, 2, 3]))
print(t[3])

empty = dict([])
print(len(empty))
