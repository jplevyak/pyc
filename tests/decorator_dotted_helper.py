def twice(f):
    def w(n):
        return f(n) * 2
    return w
