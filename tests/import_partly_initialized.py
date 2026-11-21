# A circular import that reads a name the other module has not bound yet:
# CPython raises AttributeError ("partially initialized module"); pyc reads
# the value bound later (issues/204).
import partinit_pkg
print(partinit_pkg.VAL)
