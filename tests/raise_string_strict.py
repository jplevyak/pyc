# issues/171: `raise "message"` is a Python-2 idiom that CPython 3 rejects
# with TypeError. pyc accepts it (wrapping the string in Exception(...))
# only in permissive mode, as an announced accommodation; under --strict
# the program is refused with a diagnostic naming the error.
# The permissive half is tests/raise_string.py.
def check(n):
    if n < 0:
        raise "negative not allowed"
    return n * 2

print(check(4))
