# Bytearray (CG2T_VECTOR) operations through a function call —
# the formal `b` is typed less-narrowed than the call site
# argument, so emit may see different CGv2Types at the two
# sites for the same value.  Exercises VECTOR's vec_prefix
# advance via compute_index_layout when the ptr_ty comes from
# a formal rather than a known local.

def fill_first_two(b):
  b[0] = 7
  b[1] = 42

def double_at(b, i):
  b[i] = b[i] * 2

x = bytearray(5)
fill_first_two(x)
double_at(x, 0)
double_at(x, 1)
print(x[0])
print(x[1])
print(x[2])
print(len(x))
