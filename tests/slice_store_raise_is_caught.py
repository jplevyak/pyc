# issues/175: the program's only raise is inside a builtin reached through a
# slice store, and there is no `raise`, `assert` or `yield` in user code.
# The slice-store shape arms the exception-check gate, and the store is
# followed by a check, so the ValueError reaches the handler in the caller.
def fill(dst, src):
    dst[::2] = src
    return "filled"

b = bytearray(4)
print(fill(b, bytearray(2)), b)
try:
    print(fill(b, bytearray(3)))
except ValueError as e:
    print("caught:", e)
print("end", b)
