# @staticmethod: no receiver, callable through the class, an
# instance, a subclass, and as a plain value (issue 027 feature work).
class A:
    @staticmethod
    def sf(x, y):
        return x + y

class B(A):
    pass

def main():
    print(A.sf(1, 2))
    a = A()
    print(a.sf(3, 4))
    print(B.sf(5, 6))
    m = A.sf
    print(m(7, 8))
main()
