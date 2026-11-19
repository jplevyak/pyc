# issues/199: a computed format's %r and %a give the argument's repr, for a
# str and an object alike; %s gives str(). The formats are chosen at run
# time, so the frontend cannot convert the arguments itself.
class P:
    def __str__(self):
        return "P-str"

    def __repr__(self):
        return "P-repr"


def fmt(k):
    # A format the compiler cannot see: chosen at run time.
    if k == 0:
        return "%r"
    if k == 1:
        return "[%s|%r]"
    if k == 2:
        return "%r and %r and %5.1f"
    if k == 3:
        return "%a %s"
    return "%-6r|%6s|"


n = len("abcd")
print(fmt(n - 4) % "x")
print(fmt(n - 4) % P())
print(fmt(n - 3) % ("s", "t"))
print(fmt(n - 3) % (P(), P()))
print(fmt(n - 2) % ("a", 3, 2.5))
print(fmt(n - 2) % (True, None, 1.0))
print(fmt(n - 1) % ("q", "q"))
print(fmt(n) % ("ab", "ab"))
print(fmt(n - 4) % 7, fmt(n - 4) % 0.1, fmt(n - 4) % [1, 2])
print("%%r stays %s" % "literal", fmt(n - 4) % "it's")
