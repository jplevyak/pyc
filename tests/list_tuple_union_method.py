# ifa/issues/102 class B / 030: a {list, tuple} union reaching ANY shared
# method aborts. This is shedskin_examples/sunfish's runtime failure,
# reduced to four lines.
#
# The generated C shows two candidates with the same C-level receiver
# type, so codegen cannot pick:
#
#   _CG_any  _CG_f_2304_20/*list::__pyc_getslice__*/(_CG_any a1, ...)
#   _CG_void _CG_f_2944_30/*tuple::__pyc_getslice__*/(_CG_any a1, ...)
#   assert(!"runtime error: matching function not found");
#
# It is NOT slice-specific -- len(x), x[0] and `for v in x` fail the same
# way on the same union.
#
# Note a {tuple, str} union does NOT fail, but for an accidental reason:
# FA constant-folds the str branch (its __pyc_getslice__ is emitted with
# NO parameters and a hardcoded literal), so no union ever reaches one
# call site. Both branches here are runtime values.
#
# Fixing this needs a runtime tag or fat pointer to discriminate the two
# classes -- ifa/issues/030 -- or FA precise enough that the union never
# forms.
import sys
x = [1, 2, 3, 4] if len(sys.argv) > 1 else (1, 2, 3, 4)
print(x[0:2])
