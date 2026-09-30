# issues/171 #13: dotted decorators (a module attribute, a class's
# staticmethod) are applied. They were a silent no-op: g(1) printed 2.
import decorator_dotted_helper
class D:
    @staticmethod
    def plus(f):
        def w(n):
            return f(n) + 100
        return w
@decorator_dotted_helper.twice
def g(n):
    return n + 1
@D.plus
def h(n):
    return n
print(g(1), h(1))
