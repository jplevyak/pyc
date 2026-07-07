# Issue 025 numeric unification: a variable that starts as an int
# constant and is reassigned float values in a loop resolves to
# float64 (Go/Dart-style untyped-constant coercion at the confluence,
# via AVar::num_coerce + reanalyze). Previously this produced an
# un-codegen-able {int,float} union (_CG_any / invalid casts).
# NOTE the deliberate CPython divergence documented in the shim: on a
# path where the variable never leaves its int initializer, pyc
# prints the float (0.0 not 0); these tests only exercise paths whose
# output matches CPython.
def while_accum():
    x = 0
    i = 0
    while i < 5:
        x = x * 2.0 + 1.0
        i = i + 1
    return x

def for_accum():
    total = 0
    for k in range(4):
        total = total + 0.5
    return total

def branch_join(flag):
    y = 0
    if flag:
        y = 2.5
    else:
        y = 1.5
    return y

def main():
    print(while_accum())    # 31.0
    print(for_accum())      # 2.0
    print(branch_join(True))   # 2.5
    print(branch_join(False))  # 1.5

main()
