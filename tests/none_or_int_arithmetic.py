# A {None, int64} value reaching int arithmetic: `first_negative` returns an
# int from inside its loop and otherwise falls off the end (None) -- the
# shape of shedskin_examples/pisang's `unfixed`, whose result is appended
# to a list and read back. The C backend represents
# `None | int` as an int64; the LLVM backend made every union a pointer, so
# `-x` inside the builtin abs() failed to compile under -b ("op 27
# unsupported for operand type _CG_int64"), which kept pisang off LLVM.
def first_negative(xs):
    for x in xs:
        if x < 0:
            return x
    # issues/183: falling off the end would return None, giving {None, int},
    # which cannot tell None from 0 at `abs(lit)` or `lit > 0` and is refused
    # in every mode. This test follows pisang, whose `unfixed` was edited the
    # same way because its None is unreachable (PYC_CHANGES.md).
    raise ValueError("no negative element")

fixedt = [0] * 10
mods = [3]
mods.append(first_negative([4, -7, 2]))
for lit in mods:
    fixedt[abs(lit)] = int(lit > 0)
print(fixedt)
