# Two range instances merged through a branch: the receiver union
# reaches __iter__/__pyc_more__/__next__ in ONE contour, which no
# violation-driven splitter stage separates -- the PER_CS_RECEIVER
# precision stage (ifa/issues/045) splits the method contours per
# receiver CS so each range's constants stay foldable.
def f(c):
    if c:
        r = range(2)
    else:
        r = range(3)
    t = 0
    for x in r:
        t += x
    return t
print(f(True))
print(f(False))
