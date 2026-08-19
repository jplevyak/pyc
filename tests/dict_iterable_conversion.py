# issues/110: list(d) / tuple(d) iterate a dict's KEYS.
#
# Only the two ITERATOR classes in __pyc__/07_dict.py defined
# __pyc_tolist__, never `dict` itself, so `list(d)` aborted at runtime
# with "getter not resolved" while `list(d.keys())` worked -- and
# `tuple(d)` was the last hole in make_seq's iterable surface.
#
# Sorted before printing: pyc's dict preserves insertion order here,
# but the point of the test is the CONVERSION, not the ordering.
d = {}
d[3] = "c"
d[1] = "a"
d[2] = "b"

ks = list(d)
ks.sort()
print(len(ks), ks)

vs = list(d.values())
vs.sort()
print(len(vs), vs)

# the same shape reached through tuple(), which routes via make_seq
# when PYC_MAKESEQ/PYC_TUPLE_AS_LIST are on and __pyc_tolist__ when
# they are not -- both must agree on the element count.
print(len(tuple(d)))

for k in sorted(d):
    print(k, d[k])
