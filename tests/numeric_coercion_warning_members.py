# The permissive numeric-coercion warning names the members it actually
# widened. It used to say "holds both int and float" whatever they were,
# so a {bool, int} mix -- where `True` prints as `1` -- was misreported,
# and a global's warning was placed on the PREVIOUS statement (or dropped)
# because the assignment's MOVE carried no source location.
import sys

c = len(sys.argv)
x = True
if c > 5:
    x = 7
print(x)
y = 1
if c > 5:
    y = 2.5
print(y)
z = False
if c > 5:
    z = 3
if c > 6:
    z = 4.5
print(z)
