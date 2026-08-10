# issues/041: found while porting colorsys (`hue % 1.0`). Two
# separate, pre-existing bugs in `%`, fixed together:
# 1. float % float / mixed int-float was unimplemented (compiler
#    assert at codegen and at compile-time constant-folding).
# 2. even plain int % int had the wrong sign convention -- C's/
#    fmod's truncated-toward-zero remainder instead of Python's
#    floored one (result takes the divisor's sign).
print(-7 % 3)
print(7 % -3)
print(-7 % -3)
print(7 % 3)
print(-7.5 % 3.0)
print(7.5 % -3.0)
print(-7.5 % -3.0)
print(7.5 % 3.0)
print(5.5 % 2)
print(5 % 2.5)

# Same cases again through variables -- literals above are compile-time
# constant-folded (a separate code path, ifa/if1/num.cc's DO_FOLDMOD,
# from the runtime _CG_prim_mod/LLVM P_prim_mod path these exercise).
def mod(a, b):
    return a % b

print(mod(-7, 3))
print(mod(7, -3))
print(mod(-7.5, 3.0))
print(mod(7.5, -3.0))
print(mod(5, 2.5))
