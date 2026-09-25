# issues/166 (second half): a strided `del` -- `del a[i:j:k]` with k != 1.
#
# This is a DIFFERENT operation from the extended store `a[i:j:k] = v`
# that the first half fixed. CPython REMOVES the selected elements and
# shrinks the list, where a strided store may not resize at all and
# raises ValueError on a length mismatch. pyc lowered `del o[i:j]` to
# `o[i:j] = []`, so the two arrived at the runtime indistinguishable and
# __pyc_delslice__ had to pin its step to 1 -- a strided del therefore
# deleted CONTIGUOUSLY and silently returned the wrong list.
#
# `del` now routes through __pyc_delslice__ / _CG_list_delslice, so the
# step survives. Contiguous deletes are pinned here too: they go through
# the same entry point and must keep splicing and resizing exactly as
# before.

a = list(range(20))
del a[3::4]
print(a)

b = list(range(10))
del b[2::3]
print(b)

c = list(range(10))
del c[0::2]
print(c)

# Negative steps select the same SET of indices as their positive mirror.
d = list(range(10))
del d[::-2]
print(d)

e = list(range(10))
del e[8:2:-2]
print(e)

# An explicit step of 1, and no step: both contiguous, both resize.
f = list(range(10))
del f[2:5:1]
print(f)

g = list(range(10))
del g[2:5]
print(g)

# Whole-list and open-ended forms.
h = list(range(6))
del h[:]
print(h)

i = list(range(10))
del i[::3]
print(i)

# A step larger than the list selects only the first element.
j = list(range(5))
del j[::100]
print(j)

# An empty selection must leave the list untouched.
k = list(range(5))
del k[4:1:2]
print(k)

# __delitem__ still goes through the same path (step 1, one element).
m = list(range(5))
del m[2]
print(m)

# remove() is built on __delitem__; pin it too.
n = [3, 1, 4, 1, 5]
n.remove(1)
print(n)

# A strided del on a list of strings -- element size is a pointer, not an int.
s = ["a", "b", "c", "d", "e", "f"]
del s[1::2]
print(s)

# Deleting then appending must reuse the buffer correctly.
t = list(range(10))
del t[::2]
t.append(99)
print(t)
