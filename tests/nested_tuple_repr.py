# issues/119: printing a HETEROGENEOUS tuple aborted at runtime.
#
# Filed as "nested", but nesting was incidental -- the rule is that the
# element types DIFFER:
#
#     print((1, (2, 3)))     aborted   (int64 + tuple)
#     print((1, [2]))        aborted   (int64 + list)
#     print((1, "a"))        REFUSED   ("mixed basic types: ( int64 str )")
#     print(((1, 2), (3, 4)))    fine  -- homogeneous, which hid the bug
#
# tuple.__str__ was an index loop doing `self[k].__repr__()`. With a
# runtime k, `self[k]` is the union of every field type, so the per-
# element __repr__ dispatch had no single resolution: the C backend
# aborted with "matching function not found" and the LLVM backend
# silently printed `(, )`. Where the union mixed WIDTHS (int64 + str) it
# never got that far -- the BOXING check refused the program.
#
# tuple.__hash__ had the identical loop and the identical bug, under a
# comment asserting it was safe here because the result type is int
# either way. It is the DISPATCH that fails, not the result type.
#
# Fixed by generating __str__/__hash__ UNROLLED at the program's max
# tuple arity (inject_tuple_methods, python_ifa_main.cc), joining
# __eq__/__lt__ which were already unrolled for this exact reason. A
# CONSTANT index names ONE field, so every dispatch resolves and no
# element union is ever formed -- which is also why the mixed-width case
# now compiles: there is nothing to box.
#
# The slice below is the other half. A sliced tuple has RUNTIME arity, so
# none of the unrolled `n >= k` guards fold and the constant-index path
# stays live -- on a CreationSet that clone.cc had given RECORD layout
# with ZERO members ("runtime error: bad getter"). PYC_TUPLE_AS_LIST now
# defaults on so such a tuple gets LIST layout instead. That defect was
# already reachable without any of this: `s[0]` and `s == (1, 2)` on a
# slice both aborted before this change too.
print((1, (2, 3)))
print((1, [2]))
print((1, "a"))
print(("a", 1, (2, 3), [4]))
print(((1, 2), (3, 4)))
print((1, 2))
print((1,))
print(())
print(hash((1, (2, 3))) == hash((1, (2, 3))))
t = (1, 2, 3, 4)
s = t[0:2]
print(s)
print(s[0])
print(s == (1, 2))
