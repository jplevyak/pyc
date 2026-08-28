# list.append() was QUADRATIC.
#
# __pyc__/04_sequence.py's append is `_CG_list_resize(self, elemsize,
# len+1)` followed by a store, and `_CG_list_resize_internal` used to
# MALLOC a fresh buffer and memcpy the WHOLE list on every call. Building
# a list of n elements was therefore O(n^2) copies and n allocations.
#
# Measured before the fix: 200000 appends took CPython 0.01s and pyc more
# than 9 seconds without finishing. shedskin_examples/collatz builds a
# 131072-entry lookup table this way and never got past its own setup --
# it timed out at 200s having printed nothing, after compiling with ZERO
# warnings on both backends. Afterwards it runs in 34s (CPython: 36s) and
# its output is byte-identical.
#
# The header already had the slot for the fix: `total_len` was written
# everywhere and read NOWHERE, always just set equal to `len`. It now
# means what its name says -- the allocated capacity -- and resize grows
# it geometrically, so append is O(1) amortized.
#
# This test is a CORRECTNESS test, not a benchmark: it would have run
# forever rather than failed, so what it pins is that a list built by
# repeated append still holds the right elements, that a list which
# shrinks and regrows reuses its capacity correctly, and that the
# elements survive a resize that crosses the growth boundary. (The old
# code also sized its copy by the OLD length while allocating for the
# new one, so shrinking read and wrote past the end of the fresh
# buffer; `del` exercises that path here.)
n = 20000
a = []
for i in range(n):
    a.append(i * 3)
print(len(a), a[0], a[1], a[n - 1], a[n // 2])

# shrink well below capacity, then regrow into the space already held
del a[10:]
print(len(a), a)
for i in range(50):
    a.append(-i)
print(len(a), a[0], a[9], a[10], a[59])

# a list of strings, so the element size is a pointer rather than an int
s = []
for i in range(5000):
    s.append("x" * (i % 3))
print(len(s), s[0], s[1], s[2], s[4999])

# append onto a list that started life as a literal (inline storage)
b = [1, 2, 3]
for i in range(1000):
    b.append(i)
print(len(b), b[0], b[2], b[3], b[1002])
