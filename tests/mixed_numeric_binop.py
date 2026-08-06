# issue 077 follow-up: cg.cc's emit_send_default_prim type-mismatch
# guard (added to fix a genuine pointer/scalar mismatch crash) briefly
# regressed ordinary mixed-numeric arithmetic -- comparing raw
# c_type() STRINGS flagged int64 vs float64 as a "mismatch" even
# though C's own arithmetic promotion makes int-times-float
# completely valid. Found via shedskin_examples/yopyra/yopyra.py's
# int.__mul__ fallback aborting at runtime on a genuine `2 * <float>`.
print(2 * 1.5)
print(1.5 * 2)
print(2 + 1.5)
print(5 - 1.5)
print(2 == 2.0)
print(2 < 2.5)
