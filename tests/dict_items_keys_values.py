# issues/025 "has no type" bucket: dict had no .keys()/.values()/
# .items() at all -- loop.py, mastermind2.py (via defaultdict, see
# pyc_lib/collections.py), plcfrs.py, and sunfish.py all independently
# hit this exact gap in the shedskin corpus. Covers direct iteration
# (the loop.py/mastermind2.py shape) and list(...)-wrapping (the
# sunfish.py/plcfrs.py shape, which additionally needs __pyc_tolist__
# on the iterator classes -- no plain iterator class defined that
# before this fix, a separate, narrower piece of the same gap).
#
# Deliberately does NOT combine sorted() on a plain list-of-str with
# list(d.items())+sorted(d.items()) in the same file -- that specific
# combination hits a separate, pre-existing, general FA non-convergence
# bug (any two sufficiently-different-element-type sorted() call sites
# co-occurring with a __pyc_tolist__-materialized dict-items list), not
# specific to dict and not hit by any of the real corpus examples
# above (each only does one kind of dict access). See
# ifa/issues/057-sorted-tolist-fa-nonconvergence.md.
d = {"a": "x", "b": "y", "c": "z"}
ks = sorted(d.keys())
for k in ks:
    print(k)
vs = sorted(d.values())
for v in vs:
    print(v)
for k in ks:
    print(k, d[k])
for k, v in d.items():
    print(k, v)
print(list(d.values()))
print(list(d.keys()))
print(list(d.items()))
