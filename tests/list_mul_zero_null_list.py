# ifa/166: NULL is a legal `_CG_list` and means EMPTY. `list * 0`
# produces one (`_CG_list_mult_internal` returns 0 for a zero repeat
# count), and GC_MALLOC zeroing leaves a never-assigned list field NULL.
#
# The C backend has always honoured that -- `_CG_prim_len` is
# `((_l) ? _CG_list_len(_l) : 0)` -- but the LLVM backend's emit_send_len
# loaded the length header unconditionally, so every use of such a list
# segfaulted under -b while the C backend printed the right answer. This
# test therefore only means anything when run on BOTH backends.
a = [1] * 0
print(len(a))

b = [None] * 0
print(len(b))

n = 0
c = ["x"] * n
print(len(c))

# iteration, which reads the same header
total = 0
for x in a:
    total = total + x
print(total)

# and a zero repeat reached through a function, where the count is not a
# literal the optimiser can fold
def rep(xs, k):
    return xs * k


print(len(rep([7, 8], 0)))
print(len(rep([7, 8], 2)))
