# A field holding a different type in each object needs no declaration:
# C gets one layout per type stored in `value` (int, str, float, and the
# C(C(...)) chain). This was tests/pyc_declare.py, whose
# `value = __pyc_declare__` class attribute changed nothing and was removed
# with the marker on 2026-10-16; closed issues cite it by its old name.

class C:
    def __init__(self, val):
        self.value = val

# Basic polymorphic fields
c1 = C(1)
c2 = C("2")
c3 = C(2.5)
print(c1.value)
print(c2.value)
print(c3.value)

# Nested class instantiation
c1 = C(1)
c2 = C("2")
c3 = C(1.1)
c4 = C(C(C(C(c3))))
c3.value = 2.5
print(c1.value)
print(c2.value)
print(c3.value)
print(c4.value.value.value.value.value)
