# issues/173 #1: isinstance over a union the analysis did not fold, where a
# run-time answer exists. Its counterpart, tests/isinstance_dynamic.py, is
# the union with no answer ({list, str} against list), which is refused.
class A:
    pass

class B:
    pass

class C(A):
    pass

def is_a(v):
    if isinstance(v, A):
        return 1
    return 0

# Unrelated classes: both carry a class tag.
for v in [A(), B()]:
    print(is_a(v))

# A subclass instance is an instance of its base.
for v in [C(), B()]:
    print(is_a(v))

# One type per call: the contours split, and each test folds.
def is_list(v):
    if isinstance(v, list):
        return 1
    return 0

print(is_list([1, 2]))
print(is_list("hello"))
