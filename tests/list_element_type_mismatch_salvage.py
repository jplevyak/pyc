# issue 035: P_prim_set_index_object (both the general list branch
# and the fixed-size tuple-list constant-index branch) used to cast
# the assigned value directly into the list/field's element type with
# no compatibility check -- when a salvage-degraded (or genuinely
# heterogeneous) element type ended up pointer-representable (e.g.
# boxed/generic) while the value being stored resolved to a scalar,
# or vice versa, the cast was invalid C, producing a raw compile
# error instead of the established runtime-assert salvage convention
# (issue 056's precedent, same call sites, the index argument rather
# than the value). Found via shedskin_examples/tictactoe/tictactoe.py.
# This exercises the ordinary, uniformly typed case (both branches)
# to confirm the fix didn't disturb normal list/tuple-list mutation.
a = [0, 0, 0]
a[0] = 1
a[2] = 3
print(a)

b = ["x", "y", "z"]
b[1] = "w"
print(b)
