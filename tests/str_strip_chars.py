# str.strip/lstrip/rstrip, with and without a chars argument (issues/025:
# go reads `sys.stdin.readline().rstrip('\n').rstrip('\r')`).
s = "  \t hello world \n\r "
print("[" + s.strip() + "]")
print("[" + s.lstrip() + "]")
print("[" + s.rstrip() + "]")
line = "move e4\r\n"
print("[" + line.rstrip("\n").rstrip("\r") + "]")
print("[" + "xxabcxyx".strip("xy") + "]")
print("[" + "xxabcxyx".lstrip("x") + "]")
print("[" + "xxabcxyx".rstrip("xy") + "]")
print("[" + "aaaa".strip("a") + "]")
print("[" + "".rstrip() + "]")
print("[" + "abc".strip("") + "]")
print("[" + "\x0b\x0cv\x1f".strip() + "]")
