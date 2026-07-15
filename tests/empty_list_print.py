# Issue 040: an empty list literal next to a non-empty one used to
# force the empty clone's dead __str__ loop body through
# type-checking (NOTYPE warnings; wrong-code C in larger programs).
# Fixed by hard per-constant contours for clone_methods_per_cs
# classes (range's ctor), per-constant instance CSs (no split-parent
# CS reuse for flagged classes), and the PER_CS_RECEIVER precision
# split stage (ifa/issues/045).
b = [2, 3]
print(b)
k = []
print(k)
