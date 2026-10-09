# bytearray had no __pyc_getslice__ / __pyc_setslice__: softrender's
# `self.components[:] = self.reset` did not compile.
a = bytearray(6)
for i in range(6):
    a[i] = i + 1
r = bytearray(6)
print(a[1:4], a[::2], a[::-1], a[-2:], a[4:1:-1], a[10:], a[:])
b = bytearray(6)
b[:] = a
print(b)
b[::2] = bytearray(3)
print(b)
b[1:] = b[:5]
print(b)
b[:] = r
print(b)
c = bytearray(3)
c[0:3] = b"xyz"
print(c)
c[::-1] = [1, 2, 3]
print(c)
