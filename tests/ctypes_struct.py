# issues/188: ctypes Structure and POINTER. A Structure is C-layout storage
# with a property per field; POINTER(S) is a nominal pointer type. `x`,
# `name` and `tag` are also PLAIN fields of ordinary classes here, so the
# field accessors must be chosen per receiver class.
from ctypes import CDLL, POINTER, Structure, byref, pointer, c_int, c_long, c_double, c_char_p

class Point(Structure):
    _fields_ = [("x", c_double), ("y", c_double), ("tag", c_int), ("name", c_char_p)]
    def norm1(self):
        return abs(self.x) + abs(self.y)

class A:
    def __init__(self, x):
        self.x = x
        self.name = "a"
    def bump(self):
        self.x += 1

class B:
    def __init__(self):
        self.x = "bee"
        self.tag = [1, 2]

class DivT(Structure):
    _fields_ = [("quot", c_int), ("rem", c_int)]

libc = CDLL(None)
libc.div.argtypes = [c_int, c_int]
libc.div.restype = DivT

p = Point(1.5, -2.5)
p.tag = 7
p.x = 3
p.y += 0.5
p.name = b"pt"
print(p.x, p.y, p.tag, p.name, p.norm1())
a = A(10)
a.bump()
a.x = a.x * 2
b = B()
b.x = b.x + "!"
b.tag.append(3)
print(a.x, a.name, b.x, b.tag)
q = pointer(p)
r = q.contents
r.tag = 9
print(p.tag, q[0].tag, bool(q))
d = libc.div(-7, 2)
d.rem = d.rem * 10
print(d.quot, d.rem)
ps = [Point(float(i), 0.0, i) for i in range(3)]
for z in ps:
    z.x += 1.0
print([z.x for z in ps], sum([z.tag for z in ps]))

class TM(Structure):
    _fields_ = [("tm_sec", c_int), ("tm_min", c_int), ("tm_hour", c_int), ("tm_mday", c_int),
                ("tm_mon", c_int), ("tm_year", c_int), ("tm_wday", c_int), ("tm_yday", c_int),
                ("tm_isdst", c_int), ("tm_gmtoff", c_long), ("tm_zone", c_char_p)]

class Opaque(Structure):
    pass

libc.timegm.argtypes = [POINTER(TM)]
libc.timegm.restype = c_long
libc.fopen.argtypes = [c_char_p, c_char_p]
libc.fopen.restype = POINTER(Opaque)
libc.fclose.argtypes = [POINTER(Opaque)]

t = TM(tm_year=100, tm_mon=0, tm_mday=1)
print(t.tm_year, t.tm_mday, t.tm_hour, t.tm_zone is None)
print(libc.timegm(byref(t)), libc.timegm(pointer(t)))
print(t.tm_wday, t.tm_yday)
f = libc.fopen(b"/nonexistent/pyc/ctypes", b"r")
print(bool(f), f is None)
g = libc.fopen(b"/dev/null", b"r")
if g:
    print("open", libc.fclose(g))
