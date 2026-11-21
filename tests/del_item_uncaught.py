# issues/205: an uncaught `del l[i]` out of range stops the program, as
# CPython's IndexError does, in a program with no try or raise of its own.
l = [1]
del l[3]
print("not reached")
