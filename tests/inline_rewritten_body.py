# float(int) reaches int.__pyc_float__ (`return self + 0.0`) through the
# synthesized __coerce__. After simple_inlining's first pass, sub_constants
# has replaced that body's `__operator`/`+` rvals with Vars whose SYM is the
# symbol and whose Var::constant is null; the second pass then inlined the
# body again and asserted in inline_single_pnode. The second pass never ran
# in production (the fixed-point loop counted telemetry events), only with
# --html / --dump-json -- see ifa/optimize/inline.cc inline_rval_constant.
def f(n):
  return float(n)

print(f(3))
print(float(7) / 2)
