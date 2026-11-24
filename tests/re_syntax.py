# re (issues/210): backreferences, lookaround, escape and re.error.
# Output is CPython's. Split from one test that took 24 s to compile
# (ifa/196's SETTER cascade; see issues/210).
import re


# backrefs and lookaround
print(re.findall(r'(\w)\1', "aabbcd"), re.search(r'(?P<q>["\']).*?(?P=q)', "say 'hi' ok").group())
print(re.findall(r'\w+(?=,)', "a, b, c"), re.findall(r'(?<!-)\b\d+', "1 -2 3"), re.findall(r'(?<=\$)\d+', "$5 6 $7"))

# escape, errors
print(re.escape('a.b-c d#e'))
for bad in ['(a', 'a)', '*', r'\q', '[a', 'a{3,2}', '(?<=a+)b']:
    try:
        re.compile(bad)
        print("no error", bad)
    except re.error as e:
        print("error:", e)
