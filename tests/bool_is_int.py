# bool is an int subtype (issues/214): isinstance and issubclass say so,
# narrowing sends a bool down the int branch, and a bool receiver has int's
# operators (it had no >> << // % ** / unary - at all; ~ too, which
# CPython deprecates on a bool, so it is not printed here).
print(isinstance(True, int), isinstance(False, float), isinstance(1, bool), issubclass(bool, int), issubclass(int, bool))

def kind(x):
    if isinstance(x, int):
        return "int"
    return "other"

print(kind(True), kind(3), kind("s"), kind(2.0))

def shift(x):
    if isinstance(x, int):
        return x >> 1
    return 0

print(shift(True), shift(6))
b = True
f = False
print(b >> 1, b << 3, b // 2, b % 2, b ** 2, b / 2, -b, +b, f // 3, -f, b.bit_count())
print(b + 1, 3 - b, b * 2.5, 7 // b, b & 3, f | 4, b & f, b | f)
x = 10
x >>= b
print(x)
