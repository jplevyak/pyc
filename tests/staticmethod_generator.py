# issues/181: a @staticmethod generator. pyc built no generator object for
# it, so C.s() returned the raw coroutine handle.
class C:
    @staticmethod
    def s():
        yield 3
        yield 4

for v in C.s():
    print(v)
