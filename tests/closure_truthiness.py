# ifa/issues/089: a first-class function/closure value had no
# __pyc_to_bool__ dispatch candidate at all -- `object` defines it,
# but closures never connect to `object`'s Python-specific class
# hierarchy, only to __pyc_any_type__ (ifa's own universal top type,
# sym_any). `if fn:` / bool(fn) failed to type for any closure.
def make_zero():
    return 0

factory = make_zero
if factory:
    print("yes")
else:
    print("no")

print(not factory)


class C:
    def __init__(self, factory=None):
        self.factory = factory
    def check(self):
        return self.factory()


c = C(make_zero)
if c.factory:
    print(c.check())
