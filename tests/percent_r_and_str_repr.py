# %r in %-formatting (vsnprintf printed it literally and consumed no
# argument, shifting the rest), and CPython's str repr quoting/escaping.
print("a=%.2f %r %s n=%d" % (1.5, [1, 2], "x", 3))
print("%r|%r|%s" % ("q", 5, [1]))
print("%r" % "it's", "%5r|" % 7, "100%% %r" % (None,))
print("%-8r|%s" % ("ab", [3]))
print(repr("abc"), repr("it's"), repr('say "hi"'), repr("both ' and \""), repr("a\nb\tc\\d"), repr(""))
print(["x", "y'z", "\x01"], ("t",), {"k": "v"}, repr("héllo"))
