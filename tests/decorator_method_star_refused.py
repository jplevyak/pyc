# issues/007: a decorated method pyc cannot give a slot (here `*args`) is
# refused; the decorator used to be dropped without a diagnostic.
def deco(f):
    def w(self, a):
        return f(self, a) + 1
    return w
class C:
    @deco
    def m(self, *args): return args[0]
print(C().m(1))
