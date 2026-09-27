# `~x` (P_prim_not) was not lowered by the LLVM backend: every value
# computed from it was silently dropped, so this loop never changed A and
# SHA-1's `(~B) & D` round vanished (shedskin_examples/sha's digests were
# all wrong on LLVM).
def tr():
    A, B = 5, 7
    for t in range(0, 3):
        TEMP = A + ((B & 3) | ((~B) & 12))
        B = A
        A = TEMP & 0xFFFFFFFF
    print(A, B, ~A, ~0, ~-1)
tr()
