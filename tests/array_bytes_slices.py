# array: fromlist/frombytes/fromfile/tolist, slices that are arrays, del of a slice (rdb).
from array import array
a = array("B")
a.fromlist([0, 0, 0, 1, 6] + [0] * 4)
a.frombytes(b"\x05\xff")
print(a.tolist(), len(a))
h = a[:3]
print(h.tolist(), h.typecode, a[2:8:2].tolist(), a[-2:].tolist())
del a[5:]
print(a.tolist())
del a[:]
print(len(a))
s = array("h")
s.frombytes(b"\x01\x00\xfe\xff")
print(s.tolist(), s.tobytes())
f = open("ar2.bin", "wb")
array("B", [1, 2, 3, 4, 5]).tofile(f)
f.close()
g = open("ar2.bin", "rb")
x = array("B")
x.fromfile(g, 3)
print(x.tolist())
try:
    x.fromfile(g, 5)
except EOFError:
    print("EOFError", x.tolist())
g.close()
b = array("b")
b.frombytes(b"\x80\x7f")
print(b.tolist())
del b[0]
print(b.tolist())
