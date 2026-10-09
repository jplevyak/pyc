# issues/174: `fmt % t` with t a tuple VARIABLE or field, a runtime-length
# tuple (tuple(list), a slice), a single value, or the wrong count. Each
# non-display operand goes through __pyc_fmtargs__, so %s / %r convert per
# element as for a literal tuple. Before, a tuple variable reached the
# format raw: %s read an int as a char* (segfault) and a runtime-length
# tuple was passed as one pointer.
class P:
    def __str__(self):
        return "P!"

print("<%s %s>" % (1, 2))
t = (1, 2)
print("<%s %s>" % t)
t2 = (P(), 7)
print("<%s %s>" % t2)
t3 = tuple([1, 2])
print("<%d %d>" % t3)
print("(%s %s)" % t3)
t4 = tuple([1, 2, 3])[1:]
print("[%s|%s]" % t4)
x = 42
print("x=%s" % x, "d=%d" % x, "r=%r" % "q")
one = (5,)
print("one=%s" % one)
print("%.2f and %s" % (3.14159, "pi"))
try:
    print("%s %s" % (1, 2, 3))
except TypeError as e:
    print("TypeError", e)
try:
    print("%s %s" % x)
except TypeError as e:
    print("TypeError", e)
