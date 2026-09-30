# issues/171 #12: the recursion reads the decorated value from a field written
# once, after decoration. A later rebinding of the name would not reach it,
# so it is refused rather than silently diverging from CPython.
def counted(f):
    def w(n):
        return f(n)
    return w
def outer():
    @counted
    def down(n):
        return 0 if n == 0 else down(n - 1)
    r = down(2)
    down = 5
    return r
print(outer())
