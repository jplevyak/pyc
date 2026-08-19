# issues/113: the module resolves but has no such name and no such
# submodule. CPython raises ImportError; pyc used to bind nothing and
# let the name surface later at its USE site as "'x' has no type".
from os import nosuchname

print(nosuchname(1))
