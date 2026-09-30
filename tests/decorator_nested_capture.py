# issues/171 #12: a decorated nested def that captures an enclosing local.
# The decorated path skipped closure conversion, and FA aborted
# (unique_AVar: Assertion 'es').
def counted(f):
    def wrapper(n):
        return f(n)
    return wrapper
def outer(k):
    @counted
    def down(n):
        return n + k
    return down(3)
print(outer(9))
