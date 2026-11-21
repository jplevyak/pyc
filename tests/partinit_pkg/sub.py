import partinit_pkg

# Runs during `from . import sub`, before the package binds VAL.
try:
    print(partinit_pkg.VAL)
except AttributeError:
    print("AttributeError")
