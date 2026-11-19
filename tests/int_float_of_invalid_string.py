# issues/176: int() and float() of a string accept the whole string or
# raise ValueError with CPython's message. They used to parse the longest
# valid prefix (strtoll / strtod) and return 0 when there was none.
def show_int(s):
    try:
        print(repr(s), int(s))
    except ValueError as e:
        print(repr(s), "ValueError:", e)


def show_int_base(s, b):
    try:
        print(repr(s), b, int(s, b))
    except ValueError as e:
        print(repr(s), b, "ValueError:", e)


def show_float(s):
    try:
        print(repr(s), float(s))
    except ValueError as e:
        print(repr(s), "ValueError:", e)


for s in ["12", " 7 ", "-4", "+3", "q", "", "  ", "3.5", "1_000", "1__0", "_1", "1_", "0010"]:
    show_int(s)
for s, base in [("ff", 16), ("0xff", 16), ("0x_ff", 16), ("0b101", 2), ("777", 8), ("0o17", 0),
             ("0x1A", 0), ("010", 0), ("0", 0), ("z", 36), ("12", 2)]:
    show_int_base(s, base)
for s in ["1.5", " -2.25 ", "1e3", "1_0.5", "inf", "-Infinity", ".5", "5.", "x", "", "1e", "0x1p3", "1.2.3"]:
    show_float(s)
for bs in [b"42", b"q"]:
    show_int(bs)
for bs in [b"2.5", b"x"]:
    show_float(bs)
print(sum(map(int, ["1", "2", "3"])))
try:
    print(list(map(int, ["1", "two"])))
except ValueError as e:
    print("map ValueError:", e)
print(int(3.99), int(-3.99), int(True), float(7), float(False))
