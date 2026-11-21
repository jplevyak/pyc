# calling an object whose __call__ has *args, defaults, keyword-only or **kw parameters (issues/207).
class C:
    def __call__(self, *args):
        return len(args)
class K:
    def __call__(self, a, b=10, *rest, scale=1, **kw):
        return (a + b + len(rest)) * scale + len(kw)
c = C()
k = K()
print(c(1), c(1, 2), C()("a", "b", "c"), c())
print(k(1), k(1, 2), k(1, 2, 3, 4), k(1, scale=3), k(a=5), k(1, 2, x=1, y=2))
