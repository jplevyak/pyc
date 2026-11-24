# re (issues/210): Match and Pattern attributes, flags, and the old engine's bugs
# (fullmatch, '.', [\D], (a*)*).
# Output is CPython's. Split from one test that took 24 s to compile
# (ifa/196's SETTER cascade; see issues/210).
import re


# match objects
m = re.search(r'(?P<k>\w+)=(?P<v>\w*)(x)?', ' ab=12 ')
print(m, m.lastindex, m.lastgroup, m.groups(), m.groupdict(), m.group(3), m.span('v'))
print(m['k'], m[2], m.start(), m.end(), m.groups('-'), m.expand(r'\g<v>:\1'))
print(re.match(r'(a)|(b)', 'b').groups())

# flags
print(re.findall(r'^\w+', "one\ntwo\nthree", re.M))
print(re.findall(r'a.c', "a\nc abc", re.S), re.findall(r'a.c', "a\nc abc"))
print(re.match(r'HELLO', 'hello', re.I).group(), re.match(r'(?i)[a-c]+', 'AbC').group())
print(re.search(r'''
    (\d{3})   # area
    [-.]?
    (\d{4})   # number
''', "call 555-1234 now", re.X).groups())
print(re.compile('a', re.M | re.I), re.compile('a').flags, re.compile('(?i)a').flags)

# fixed bugs
print(re.fullmatch(r'a|ab', 'ab'))
print(re.match(r'.', '\n'), re.findall(r'[\D]', 'a1b2'), re.findall(r'[^\W\d]+', 'ab12 c_d'))
print(re.match(r'(a*)*', 'b').groups(), re.match(r'(a|b)*?c', 'abac').group())
