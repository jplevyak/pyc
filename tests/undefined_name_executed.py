# issues/107: an undefined name that is actually EXECUTED.
#
# CPython:  NameError: name 'NoSuchName' is not defined
# pyc:      three warnings, exit code 0, and a binary that SEGFAULTS.
#
# None of pyc's warnings names the real problem -- they report analyser
# state ("'NoSuchName' has no type", "expression has no type") rather
# than the user's mistake. The target is a hard compile error naming the
# symbol, which pyc can do statically and strictly better than CPython's
# runtime NameError.
print(NoSuchName)
