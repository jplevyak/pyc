# issues/050: str.join/upper/lower/swapcase/replace and substring reads build
# their result in one allocation (they concatenated one char at a time, O(n^2)).
# Outputs checked against CPython, including replace() with an empty 'old'.
def gen(n):
    for i in range(n):
        yield str(i)
print("-".join(["a", "bc", "", "d"]))
print(",".join(gen(5)))
print("".join([]), "x".join(["only"]))
print("Hello, World! 123".upper(), "Hello, World! 123".lower(), "Hello, World! 123".swapcase())
print("aaa".replace("a", "bb"), "abcabc".replace("bc", ""), "abc".replace("", "-"), "abc".replace("x", "y"))
print("aaaa".replace("aa", "b"), "".replace("", "z"), "a\x00b".replace("\x00", "0"))
s = "the quick brown fox"
print(s.split(" "), s.split(), s[4:9])
from io import StringIO
import re
import os
f = StringIO("line one\nline two\ntail")
print(f.readline(), f.read(3), f.read())
m = re.match(r"(\w+) (\w+)", "hello world again")
print(m.group(1), m.group(2))
print(os.path.split("/a/b/c.txt"), os.path.splitext("c.tar.gz"))
big = "".join([chr(97 + i % 26) for i in range(200000)]).upper().replace("A", "")
print(len(big), big[:5])
