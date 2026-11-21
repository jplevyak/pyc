# A str's length, indexing, slicing and iteration count characters (code
# points) in CPython. pyc's str is UTF-8 bytes, and these count bytes
# (issues/203).
s = "héllo 世界"
print(len(s), s[1], s[-1], s[1:3], s[::-1])
print([c for c in "été"], "été".find("t"), "naïve".upper())
