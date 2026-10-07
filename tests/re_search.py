# re.search and Pattern.search (minilight's camera/scene/triangle parsers).
# They were left out of pyc_lib/re.py because two compiler bugs (ifa/040,
# closed) once broke a Pattern matched more than once.
import re

SEARCH = re.compile(r'(\(.+\))\s*(\(.+\))\s*(\S+)')
for line in ["(0.278 0.275 -0.789) (0 0 1) 40", "  (1 2 3)   (4 5 6)  10", "none here"]:
    m = SEARCH.search(line)
    if m is None:
        print("no match")
    else:
        p, d, a = m.groups()
        print(p, d, float(a), m.start(0), m.end(0))
print(re.search(r'b+', "aaabbbc").group(0), re.search(r'z', "abc") is None)
P = re.compile(r'ab+')
print(P.match("abbb").group(0), P.match("abx").group(0), P.search("xxab").start(0))
