# A package that imports its own submodule, which imports the package back
# (tests/import_circular.py).
from . import sub


def base():
    return "base"


def twice():
    return sub.leaf() + sub.leaf()
