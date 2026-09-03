# issues/123: a class defining __repr__ but not __str__ must render via
# __repr__ for both print() and str(). pyc prints "<object>" -- silently,
# no warnings, exit 0. `go`'s board is drawn this way.
class C:
    def __repr__(self):
        return "hello"


# The reverse must NOT become symmetric: a class defining __str__ only
# keeps the DEFAULT repr(), so this cannot be fixed with an alias.
class D:
    def __str__(self):
        return "dee"


print(C())
print(str(C()))
print(D())
