# re (issues/210): what pyc refuses at run time instead of answering
# differently from CPython. findall with two or more groups returns tuples
# (a type that depends on the group count, known only at run time), and
# Match.lastindex is None when no group matched (an int that may be None).
# CPython answers both, so its output differs: .python.expect_fail.
import re
try:
    re.findall(r'(\w+)=(\w+)', "a=1")
except NotImplementedError as e:
    print("refused:", e)
m = re.match(r'a', 'a')
try:
    print(m.lastindex)
except NotImplementedError as e:
    print("refused:", e)
print(m.lastgroup)
