# ifa/issues/092: 3-arg min/max used to silently misbind the 3rd
# positional value into the `key=` formal (no third slot existed),
# then crash at runtime trying to call that value as a function.
print(max(3, 7, 1))
print(max(-1, -7, -3))
print(max(3.5, 7.25, 1.0))
print(min(3, 7, 1))
print(min(-1, -7, -3))
print(min(3.5, 7.25, 1.0))

# Must not regress the pre-existing 1-arg-iterable / 2-arg / key=
# forms -- these share the same function, just a different contour.
print(max([5, 2, 9, 1]))
print(min([5, 2, 9, 1]))
print(max(2, 9))
print(min(2, 9))
print(max([5, 2, 9, 1], key=lambda x: -x))
print(min([5, 2, 9, 1], key=lambda x: -x))

def f(r, g, b):
    maxc = max(r, g, b)
    minc = min(r, g, b)
    if r == g:
        return maxc, minc, 1.0
    return maxc, minc, 2.0

print(f(0.5, 0.5, 0.9))
