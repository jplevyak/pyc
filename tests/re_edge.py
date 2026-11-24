# re (issues/210) edge cases: CPython 3.7's empty-match rules, pos/endpos,
# anchors, case folding, quantifiers, class escapes, templates and the
# messages of re.error. Output is CPython's.
import re

def show(x):
    print(repr(x))

# empty matches and adjacency (CPython 3.7 rules)
show(re.sub(r'\b', '|', 'ab cd'))
show(re.sub(r'a*', 'X', 'baaac'))
show(re.findall(r'a*?', 'aa'))
show(re.split(r'(?=b)', 'abab'))
show(re.sub(r'', '-', 'abc'))
show(re.findall(r'', ''))
show(re.split(r'a', ''))

# anchors and bounds
show(re.search(r'c$', 'abc\n').span())
show(re.findall(r'\w$', 'a\nb\n', re.M))
show(re.search(r'\Aab', 'xab'))
show(re.search(r'b\Z', 'ab\n'))
p = re.compile(r'\d+')
show(p.search('a12b345', 3).group())
show(p.match('a12', 1).group())
show(p.search('12345', 0, 3).group())
show(p.findall('1 22 333', 2, 6))
show(p.fullmatch('a123b', 1, 4).group())
show(re.compile(r'^b').search('ab', 1))
show(p.search('abc', 10))

# case folding
show(re.findall(r'[a-z]+', 'Hello WORLD', re.I))
show(re.match(r'(a)\1', 'aA', re.I).group())
show(re.findall(r'[^a-z]', 'aB1', re.I))
show(re.match(r'(?i:ab)c', 'ABc').group())
show(re.match(r'(?i:ab)c', 'ABC'))

# quantifiers
show(re.match(r'a{2,}', 'aaaa').group(), )
show(re.match(r'a{,2}', 'aaaa').group())
show(re.match(r'a{2}?', 'aaaa').group())
show(re.match(r'a{1,3}?b', 'aaab').group())
show(re.match(r'x{a}', 'x{a}').group())
show(re.match(r'(ab)+', 'ababx').groups())
show(re.match(r'(a|ab)(c|bcd)(d*)', 'abcd').groups())
show(re.match(r'((a)|b)+', 'ab').groups())
show(re.match(r'(?:(a)|b)*', 'ab').groups())

# classes and escapes
show(re.findall(r'[\s,]+', 'a, b  c'))
show(re.findall(r'[\w.-]+@[\w.-]+', 'x a.b-c@d.org y'))
show(re.findall(r'[]a]', 'a]b'))
show(re.findall(r'[\x41-\x43]', 'ABCD'))
show(re.findall('\\u0041|\\101|\\x42', 'AB'))
show(re.match(r'\t\n', '\t\n').group())
show(re.findall(r'[\b]', 'a\bb'))
show(re.findall(r'\S+', ' a\x1cb c '))

# templates
show(re.sub(r'(a)(b)?', r'[\2]', 'ac ab'))
show(re.sub(r'(\w+)', r'\g<1>\g<0>', 'hi'))
show(re.sub(r'a', r'\-\.', 'a'))
show(re.subn('a', 'b', 'aaaa', 2))

# Pattern attributes
q = re.compile(r'(?P<y>\d{4})-(?P<m>\d\d)')
print(q.pattern, q.groups, q.groupindex, q)
m = q.search('on 2024-05-01')
print(m.re.pattern, m.string, m.pos, m.endpos, m.lastgroup, m.lastindex)
show(re.compile(r'a', re.I | re.S))

# errors
for bad in ['a**', 'a{1}{2}', '(?P<1>a)', '(?P<n>a)(?P<n>b)', r'(a)\2', '(?z)', 'a(?i)b', r'[z-a]', r'\x4', '(?P=x)', '(a\\1)', '[\\d-z]']:
    try:
        re.compile(bad)
        print("no error", bad)
    except re.error as e:
        print("error:", e)
try:
    re.sub('a', r'\q', 'a')
except re.error as e:
    print("error:", e)
try:
    re.sub('(a)', r'\2', 'a')
except re.error as e:
    print("error:", e)
try:
    re.sub('(a)', r'\g<x>', 'a')
except IndexError as e:
    print("IndexError:", e)
try:
    re.search('a', 'a').group(2)
except IndexError as e:
    print("IndexError:", e)
