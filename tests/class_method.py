# @classmethod: cls receives the class the call was made through;
# cls(...) constructs (dispatches to __new__ via the class value).
class A:
    def __init__(self, v):
        self.v = v
    @classmethod
    def make(cls, v):
        return cls(v)
    @classmethod
    def make_default(cls):
        return cls(99)

def main():
    a = A.make(42)
    print(a.v)
    b = A.make_default()
    print(b.v)
main()
