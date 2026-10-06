# str.split(sep, maxsplit) and the empty-separator ValueError (rdb).
print("a=b=c".split("=", 1), "a=b=c".split("="), "a=b=c".split("=", 0), "=a==".split("="), "x".split("=", 5))
print(" a  b c  ".split(), " a  b c  ".split(None, 1), "  ".split(), "a b".split(None, 0), "".split())
print("ab".split("ab"), "aXXbXXc".split("XX", -1))
try:
    "a".split("")
except ValueError:
    print("ValueError")
