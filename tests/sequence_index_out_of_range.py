# issues/179: an out-of-range index raises IndexError. pyc read whatever
# memory followed the sequence.
import sys

i = len(sys.argv)          # 1, so nothing is constant-folded
l = [1]
s = "a"
b = b"a"
try:
    print(l[i])
except IndexError:
    print("list IndexError")
try:
    print(repr(s[i]))
except IndexError:
    print("str IndexError")
try:
    print(repr(s[-1 - i]))
except IndexError:
    print("str negative IndexError")
try:
    print(b[i])
except IndexError:
    print("bytes IndexError")
