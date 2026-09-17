# ifa/164: `[None] * n` followed by a full overwrite is the standard
# preallocation idiom (shedskin_examples/solitaire). The list element is
# TEMPORALLY {None, str} -- None at t0, str at t1 -- so no contour split
# separates it and none should be asked for. The answer is the
# representation: for a pointer-shaped union pyc already strips nil from
# the AType `->type` projection (issue 060, "Optional[pointer] still
# single-clone"), which is exactly shedskin's `list<str *>` with NULL.
#
# The primitive-argument check was the one consumer reading the RAW `out`,
# so the two operand positions of one operator disagreed: None in the
# RECEIVER compiled silently while None in an ARGUMENT was fatal. That
# rejected this program, which CPython runs without complaint.
def pad(txt):
    cipher = [None] * len(txt)
    for n in range(len(txt)):
        cipher[n] = txt[n]
    # str.join reads the element into `x` and does `r = r + x`, putting
    # the nullable union in an ARGUMENT position.
    return "".join(cipher)


print(pad("abc"))
print(pad(""))


# The nullable union in an argument to a user function, then concatenated.
def cat(xs):
    r = ""
    for x in xs:
        r = r + x
    return r


def build(n):
    a = [None] * n
    for i in range(n):
        a[i] = "ab"
    return cat(a)


print(build(3))


# A nullable CLASS union reaching a method call -- the receiver side,
# which already worked, kept here so the two positions stay symmetric.
class Node:
    def __init__(self, v):
        self.v = v

    def get(self):
        return self.v


def nodes(n):
    a = [None] * n
    for i in range(n):
        a[i] = Node(i)
    t = 0
    for x in a:
        t = t + x.get()
    return t


print(nodes(4))
