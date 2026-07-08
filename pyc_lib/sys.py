# pyc shim for the standard `sys` module (the statically-modellable
# parts). File objects (stdin/stdout/stderr) are NOT provided yet.
# See issue 025 bucket C.

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

class _StdoutStub:
    def write(self, s):
        print(s)
    def flush(self):
        pass

stdout = _StdoutStub()
