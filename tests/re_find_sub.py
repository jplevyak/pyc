# re (issues/210): findall, finditer, sub/subn with templates and functions, split.
# Output is CPython's. Split from one test that took 24 s to compile
# (ifa/196's SETTER cascade; see issues/210).
import re


# findall / finditer
print(re.findall(r'\d+', "a1 b22 c333"))
print(re.findall(r'(\w)=', "a=1 b=2 c"))
print(re.findall(r'x*', "abxd"))
print([m.span() for m in re.finditer(r'\w+', "hi there, you")])
print([m.groups() for m in re.finditer(r'(\w+)=(\w+)', "k=v a=b")])
for k, v in [m.groups() for m in re.finditer(r'(\w+)=(\w+)', "x=1 y=2")]:
    print(k, v)

# sub / subn
print(re.sub(r'x*', '-', 'abxd'))
print(re.sub(r'(\w+)@(\w+)', r'\2 at \1', "joe@home and ann@work"))
print(re.sub(r'(?P<w>\d+)', r'<\g<w>>', "a1b22"))
print(re.sub(r'\d', lambda m: str(int(m.group(0)) * 2), "a1b2c3"))
print(re.subn(r'o', '0', "foo boo", count=3))
print(re.sub('a', r'\n\t\\', 'xay'))

def up(m):
    return m.group(1).upper()
print(re.sub(r'_(\w)', up, "snake_case_name"))

# split
print(re.split(r'\s+', "a  b   c"))
print(re.split(r'(,)|;', 'a,b;c'))
print(re.split(r',', 'a,b,c,d', maxsplit=2))
print(re.split(r'\b', 'a b'))
print(re.split(r'x*', 'axbc'))
