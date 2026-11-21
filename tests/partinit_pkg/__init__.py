# A package whose submodule reads one of its names before it is bound
# (tests/import_partly_initialized.py).
from . import sub
VAL = 5
