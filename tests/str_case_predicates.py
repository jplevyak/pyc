# issues/118: str.islower / str.isspace / str.swapcase did not exist.
# Calling one produced an opaque "illegal call argument type expression"
# at compile time and "getter not resolved" at runtime -- with no hint
# that the method was simply absent. sunfish's gen_moves uses isspace
# and islower to find the board edges; every ray scan hit the missing
# method instead. ASCII-only, consistent with the existing
# upper/lower/isupper.

for s in ["z", " ", "  ", "", "aB", "ab", "AB", "a1", "1", "Hello World"]:
    print(s.isspace(), s.islower(), s.isupper(), s.swapcase())

# Whitespace characters other than the space itself.
print("\t".isspace(), "\n".isspace(), "\r".isspace(), "\x0b".isspace(), "\x0c".isspace())
print("a\tb".isspace(), " a ".isspace())

# swapcase round-trips.
t = "MiXeD Case 123!"
print(t.swapcase())
print(t.swapcase().swapcase() == t)
