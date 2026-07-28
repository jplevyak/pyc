# int(True)/int(False). The LLVM backend used to sign-extend a bool
# when widening it to a wider integer, so int(True) came out -1 (i1
# True sign-extends to all-ones) instead of 1 -- fixed by
# zero-extending bool in emit_send_coerce (cg_emit_llvm.cc).

print(int(True))                 # 1
print(int(False))                # 0
print(int(True) + int(True))     # 2
print(int(5 > 4))                # 1
print(int(2 > 9))                # 0

n = 0
for b in [True, True, False, True]:
    n += int(b)
print(n)                         # 3

m = 10
if True:
    m = m + int(3 > 1)
print(m)                         # 11
