# pyc shim for the standard `sys` module (the statically-modellable
# parts).

# No real command line is threaded through pyc, so argv is a
# single-element list (program name only). `len(sys.argv) > 1` is
# therefore False and programs take their no-argument default path.
argv = ["pyc"]

maxsize = 9223372036854775807

def exit(status=0):
    __pyc_c_call__(int, "::exit", int, status)

# Recursion limit is a CPython interpreter detail with no analogue in
# compiled code: accept and ignore.
def setrecursionlimit(n):
    return None

version = "2.7.18"

# Std streams as file objects (__pyc_file__ is the builtin file class
# from __pyc__/07_file.py; builtin-module names are globally visible).
# These replace the earlier _StdoutStub: real fd-backed streams whose
# write() does NOT append a newline (print-based stubs did).
stdin = __pyc_file__(__pyc_c_call__(int, "_CG_fstd", int, 0))
stdout = __pyc_file__(__pyc_c_call__(int, "_CG_fstd", int, 1))
stderr = __pyc_file__(__pyc_c_call__(int, "_CG_fstd", int, 2))
