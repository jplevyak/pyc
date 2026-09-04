# ifa/issues/118: a RECORD (tuple) index was emitted RAW -- no
# negative-index normalisation -- while the list path normalised. `t[-1]`
# therefore indexed BACKWARDS off the front of the object instead of from
# its end, reading memory before the allocation.
#
# Found in chess via PYC_NO_GC + valgrind: `evals[board[i]]`, where a
# board square holds a NEGATIVE code for a black piece, read 16 bytes
# before the tuple's block. Under the collector this was silent
# corruption that surfaced much later as a mangled GC free list.
#
# Both a constant and a runtime index, since only the runtime one is
# beyond the C compiler's reach.
evals = (10, 20, 30, 40)
print(evals[-1])
print(evals[-4])
i = -2
print(evals[i])
j = -3
print(evals[j])

# The same shape chess uses: a negative value read out of a list, then
# used to index a tuple.
codes = [-1, -4, 2]
table = (0, 100, 300, 330, 510)
for c in codes:
    print(table[c])
