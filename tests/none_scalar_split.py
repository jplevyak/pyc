# issue 060: None combined with a raw scalar (int/bool/float) in a
# value flowing to a shared function. IFA's core discipline is to
# split incompatible types, and None IS incompatible with a scalar:
# under the unboxed representation they share the zero bit pattern, so
# a single shared clone cannot tell None from 0/False. type_cannonicalize
# now keeps nil in the ->type projection when a scalar is also present,
# so FA splits the None caller into its own contour and `is None` folds
# statically per contour. No match/case needed -- this is the general
# bug the match `case None:` limitation was a symptom of.
def show(v):
    if v is None:
        print("none")
    else:
        print("notnone")

show(None)
show(True)
show(False)
show(5)
show(0)


# None + int, distinct non-None values, with a use of v on the else path
def classify(v):
    if v is None:
        return -1
    return v + 100


print(classify(None))
print(classify(5))
print(classify(0))


# None must still merge with a pointer type (frontend-sanctioned): a
# Node pointer is never NULL except for real None, so this stays a
# single clone and prints correctly.
class Node:
    def __init__(self, n):
        self.n = n


def field(x):
    if x is None:
        print("nil")
    else:
        print(x.n)


field(None)
field(Node(3))
field(Node(9))
