# bytes.join did not exist (minpng's `b''.join(img)`). It collects the
# iterable into a list and joins in one allocation with _CG_string_join.
# The list's only reader is that C helper, which dead-code elimination
# could not see: every element store into it was deleted and the join
# produced N empty pieces. A live opaque primitive now keeps its
# arguments' contents live.
print(b"-".join([b"x", b"y"]), b"".join([b"ab", b"c"]))
img = []
for i in range(3):
    img.append(bytes([65 + i, 66]))
print(b"".join(img), b", ".join(img), len(b"".join(img)))
def gen(n):
    for i in range(n):
        yield bytes([97 + i])
print(b"+".join(gen(4)), b"".join([]), b"|".join([b"only"]))
