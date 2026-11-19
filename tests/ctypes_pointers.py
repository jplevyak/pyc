# issues/188 step 0: a pointer object is a one-word C cell, as in CPython.
# A pointer FIELD reads as a view of the field's cell, so storing through
# it changes the struct; POINTER(POINTER(S)) is a type; byref(p) is the
# address of p's cell, and a foreign function can write it.
from ctypes import CDLL, POINTER, Structure, byref, pointer, c_int, c_size_t

class Node(Structure):
    pass
Node._fields_ = [("v", c_int), ("next", POINTER(Node))]
NodeP = POINTER(Node)
NodePP = POINTER(NodeP)

libc = CDLL(None)
# memcpy over pointer cells: copies one pointer's address into another.
libc.memcpy.argtypes = [NodePP, NodePP, c_size_t]
libc.memcpy.restype = None

a = Node(1)
b = Node(2)
c = Node(3)
print(bool(a.next))                    # a NULL field is a falsy pointer
a.next = pointer(b)
b.next = pointer(c)
n = a
total = 0
while n:
    total += n.v
    if not n.next:
        break
    n = n.next.contents
print(total)

f = a.next                             # a view of a's field
f.contents.v = 20                      # writes b
print(b.v, a.next.contents.v)

pp = pointer(a.next)                   # POINTER(POINTER(Node)) to a's field
print(pp.contents.contents.v, pp[0].contents.v)

x = pointer(c)
y = NodeP()                            # NULL
print(bool(y))
libc.memcpy(byref(y), byref(x), 8)     # 8: a pointer on the 64-bit targets pyc supports
print(bool(y), y.contents.v)
libc.memcpy(pp, byref(y), 8)           # a.next = c, through the C library
print(a.next.contents.v)
try:
    print(NodeP().contents.v)
except ValueError as e:
    print("ValueError", e)
