# issues/165, the half left open: a format string that is NOT a
# compile-time constant.
#
# The emitters cast each argument to what its conversion needs, but only
# when they can PARSE the format to know which conversion that is. With a
# computed format they cannot, so an integer was widened to int64 and a
# float was left as a double -- and that double then met `%d`, which on
# x86-64 SysV reads an integer register while the value sits in an xmm
# one. `"%d" % 3.7` through a computed format printed 25637, and a
# different number on the next run.
#
# Neither end can repair it alone (the emitter cannot see the format, the
# runtime cannot see the types), so the emitter now passes a per-argument
# TYPE TAG and _CG_format_string_tagged pairs them with the conversions it
# finds. Every case below goes through a format the compiler cannot fold.

def fmt(k):
    if k == 0:
        return "%d"
    if k == 1:
        return "[%d]"
    if k == 2:
        return "%f"
    if k == 3:
        return "%s"
    if k == 4:
        return "%x"
    if k == 5:
        return "%08.3f"
    if k == 6:
        return "%c"
    if k == 7:
        return "%5d|%-5d"
    return "%o"

# a float meeting an integer conversion -- CPython truncates toward zero
print(fmt(0) % 3.7)
print(fmt(1) % 2.9)
print(fmt(0) % -3.7)

# an int meeting a float conversion
print(fmt(2) % 5)

# the matching cases must keep working
print(fmt(0) % 42)
print(fmt(2) % 1.5)
print(fmt(3) % "hi")
print(fmt(4) % 255)
print(fmt(8) % 8)

# width, flags and precision survive the rewrite
print(fmt(5) % 3.14159)
print(fmt(7) % (7, 7))

# %c narrows where the others widen
print(fmt(6) % 65)

# a 64-bit value through a computed %d -- the original issues/165 bug,
# which the `ll` rewrite already covered but is pinned here too
print(fmt(0) % 199999990000000)

# a bool through a computed %d
print(fmt(0) % True)

# a tuple of mixed types through one computed format
def fmt2():
    return "%d %f %s"
print(fmt2() % (3.9, 4, "x"))

# A NUMBER meeting a computed `%s`. With a constant format the frontend
# pre-converts the argument through __str__; it cannot do that without the
# format, so the number arrived raw and `%s` read it AS A POINTER --
# `"%s" % 42` through a computed format SEGFAULTED, compiling clean first.
# The type tag says what it really is, so the runtime renders it.
# (An OBJECT at a computed `%s` is still wrong; see issues/168.)
def sfmt(k):
    if k == 0:
        return "%s"
    if k == 1:
        return "[%s]"
    return "%8s|"

print(sfmt(0) % 42)
print(sfmt(0) % -7)
print(sfmt(0) % 3.5)
print(sfmt(0) % 2.0)
print(sfmt(1) % "str")
print(sfmt(2) % 42)
print(sfmt(2) % "ab")
print(sfmt(0) % 199999990000000)
print(sfmt(0) % True)
