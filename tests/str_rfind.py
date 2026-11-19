# issues/197: str.rfind and str.rindex, with CPython's start/end bounds.
s = "a.b.c.d"
print(s.rfind("."), s.rfind(".", 0, 4), s.rfind(".", 2), s.rfind(".", 0, 1), s.rfind("x"))
print(s.rfind(""), s.rfind("", 3), s.rfind("", 9), s.rfind(".", -3), s.rfind(".", 0, -2))
print(s.rfind("c.d"), s.rfind("c.d", 0, 6), s.rindex("b"))
try:
    s.rindex("q")
except ValueError as e:
    print("ValueError", e)
