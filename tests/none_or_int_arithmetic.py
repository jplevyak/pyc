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

fixedt = [0] * 10
mods = [3]
mods.append(first_negative([4, -7, 2]))
for lit in mods:
    fixedt[abs(lit)] = int(lit > 0)
print(fixedt)
