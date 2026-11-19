# bytearray(n) is n zero bytes. The analysis typed a bytearray's elements
# only from writes, so one that was only ever READ had no element type and
# `bytearray(3)[0]` was refused with "expression has no type".
y = bytearray(3)
print(y[0], y, len(y))
def f(n):
    z = bytearray(n)
    return z[n - 1]
print(f(4))
print(len(bytearray(0)), bytearray(0))
