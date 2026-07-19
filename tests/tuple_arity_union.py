# issues/025 (plcfrs C-compile-error follow-up, 2026-07-19): a real,
# fixed-arity tuple literal (`(a, b)`, not a list literal promoted to
# record shape) got a bare GC_malloc with no list-header at all --
# fine as long as every consumer resolves its arity at compile time,
# but tuples of *differing* arity flowing into one union (grammar
# rules with a variable RHS length, e.g. shedskin's plcfrs.py) push
# len()/non-constant indexing onto the generic runtime fallback,
# which unconditionally reads a list-header a true tuple never had --
# garbage memory. Symptoms ranged from silently wrong output (an
# all-empty-tuple union) to a hard FA violation depending on how
# badly the garbage confused downstream inference. Fixed by giving
# every tuple a real header unconditionally (ifa/codegen/cg.cc,
# ifa/codegen/cg_emit_llvm.cc) -- harmless for tuples that were
# already working, since field access (`->eN`) is offset-based from
# the pointer forward and doesn't care what's behind it.
grid = [("a", "b", "c"), ("d", "e")]
flat = [x for row in grid for x in row]
print(flat)

ts = [(3,), (1, 2)]
print(ts)
print(len(ts[0]))
print(len(ts[1]))

# the exact issues/044 sibling repro, but with a TRUE tuple literal
# instead of a list literal (044 fixed the list-literal case only).
t = (3,), (1, 2)
print(t)
