# pyc shim for the standard `sys` module (the statically-modellable
# parts).

# Real process argv, threaded through from generated main() via
# _CG_set_argv (pyc_c_runtime.h) and read back here through opaque
# c-calls rather than a Python-level constant -- built this way (not
# `argv = [...]` with a literal) specifically so FA can't see through
# it and constant-fold `len(sys.argv)`/`sys.argv[i]` at compile time,
# which used to silently dead-code any branch keyed on a real
# command-line argument (pyc's own `8.py` example: `if len(sys.argv)
# > 1 and sys.argv[1] == "a"` always took the `else` branch,
# regardless of what was actually passed at the real command line).
def _get_argv():
    n = __pyc_c_call__(int, "_CG_argc")
    result = []
    i = 0
    while i < n:
        result.append(__pyc_c_call__(str, "_CG_argv_at", int, i))
        i += 1
    return result

argv = _get_argv()

maxsize = 9223372036854775807

def exit(status=0):
    __pyc_c_call__(int, "::exit", int, status)

# Recursion limit is a CPython interpreter detail with no analogue in
# compiled code: accept and ignore.
def setrecursionlimit(n):
    return None

# Was "2.7.18" -- a Python-2-migration leftover (issues/041); pyc
# targets Python 3 syntax/semantics, and a real corpus example
# (shedskin_examples/circle/circle.py) does `print(sys.version)`
# directly, so this was silently printing a false, actively
# misleading value. "(pyc)" instead of a real CPython build string
# since this isn't CPython -- claiming a specific patch-level CPython
# build would be its own kind of wrong.
version = "3.11.0 (pyc)"
# Indexed access (sys.version_info[0] == 3, the shape
# shedskin_examples/sunfish/sunfish.py actually uses) is fine.
# Printing the whole tuple directly hits a pre-existing, general pyc
# limitation, unrelated to this addition: a heterogeneous (mixed
# int/str) tuple has no working generic __str__ (see ifa/issues/018).
version_info = (3, 11, 0, "final", 0)

# Best-effort: distinguishes the corpus's `sys.platform == 'win32'`
# checks (shedskin_examples/circle/circle_main.py) without modelling
# every real `sys.platform` value; "linux" matches this project's
# primary supported/tested host.
platform = "linux"

# Std streams as file objects (__pyc_file__ is the builtin file class
# from __pyc__/07_file.py; builtin-module names are globally visible).
# These replace the earlier _StdoutStub: real fd-backed streams whose
# write() does NOT append a newline (print-based stubs did).
stdin = __pyc_file__(__pyc_c_call__(int, "_CG_fstd", int, 0))
stdout = __pyc_file__(__pyc_c_call__(int, "_CG_fstd", int, 1))
stderr = __pyc_file__(__pyc_c_call__(int, "_CG_fstd", int, 2))
