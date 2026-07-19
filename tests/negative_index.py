# issues/025: plain (non-slice) negative indexing had NO
# normalization at all -- str.__getitem__/list.__getitem__/
# list.__setitem__/tuple.__getitem__/tuple.__setitem__ pass the raw
# key straight through to index_object/set_index_object's C array
# indexing, which is undefined behavior for a negative key: it reads/
# writes memory *before* the buffer instead of counting back from the
# end. `a[-1]` on a real list returned a garbage value, not the last
# element -- had zero test coverage before this fix (only positive
# indices, tests/string_index.py).
#
# Fixed at the codegen level (ifa/codegen/cg.cc,
# ifa/codegen/cg_emit_llvm.cc, pyc_c_runtime.h/pyc_runtime.c's new
# _CG_norm_idx helper), not in the __getitem__/__setitem__ Python
# source: an earlier attempt at the Python-source level (mirroring
# range.__getitem__'s existing `if idx < 0: idx += len(self)`)
# broke FA's handling of an empty list literal sharing a program
# with a non-empty one (issues/025/040's "has no type" fragility) --
# even a no-op `if key < 0: pass` inside list.__getitem__ triggered
# it. A plain C ternary in the generated code, with no FA-visible
# branch at all, sidesteps that fragility.
#
# A compile-time-constant negative index into a fixed-size
# (tuple-list literal, or a real tuple) record hits a *different*
# emission path (`->eN` field access on the C backend, a flat GEP on
# the LLVM backend) -- normalized separately there using the
# record's field count (a compile-time constant), not a runtime
# length read (confirmed: an actual tuple, `_CG_prim_tuple`, has no
# list-header at all, unlike a list literal -- reusing the runtime
# list-header trick for it read garbage memory and returned nonsense,
# not a crash).
#
# bytearray (a @vector class) needed a third fix, separate again:
# its length is a runtime struct field (self.length), not a list
# header and not a compile-time constant, so neither codegen fix
# above applies. Unlike list/tuple, bytearray isn't
# clone_methods_per_cs-flagged, so it isn't exposed to issues/052's
# empty-container fragility -- fixed directly in Python source
# (__pyc__/06_bytearray.py), confirmed safe with an empty+non-empty
# bytearray in one program before landing.
a = [10, 20, 30, 40, 50]
print(a[-1])
print(a[-2])
print(a[0])
print(a[4])

a[-1] = 99
print(a)
a[-2] = 88
print(a)

s = "hello"
print(s[-1])
print(s[-2])

t = (10, 20, 30, 40, 50)
print(t[-1])
print(t[-2])
print(t[0])
print(t[4])

# dynamic (non-constant) negative index too, not just a literal.
i = -1
print(a[i])
print(t[i])
print(s[i])

# bytearray (@vector class): fixed at the Python source level
# (unlike list/tuple above -- bytearray isn't clone_methods_per_cs
# flagged, so it isn't exposed to issues/052's fragility; confirmed
# empirically with an empty+non-empty bytearray in one program
# before landing this).
ba = bytearray(5)
ba[0] = 1
ba[1] = 2
ba[-1] = 9
print(ba[-1])
print(ba[-2])
print(ba[0], ba[1], ba[2], ba[3], ba[4])
