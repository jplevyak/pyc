# Tier 3 stress test: a small expression evaluator.
#
# Started as a polymorphic OOP test:
#     class Expr; class Const(Expr); class BinOp(Expr)
#     each subclass overrides eval(self)
#     evaluate via `expr.eval()` dispatching to the right subclass
#
# That pattern hits two pyc limitations:
#   1. Multi-target method dispatch is unsupported in
#      codegen.  `get_target_fun_core` returns null when
#      `fns->n != 1`; write_send then emits
#      `assert(!"matching function not found")`.  The
#      receiver carries method pointers in its struct
#      (e.g. e2 = eval) so indirect dispatch would be
#      feasible, but pyc currently inlines the call to
#      the resolved Fun and gives up if there's more
#      than one.
#   2. `isinstance(x, Class)` for non-None classes
#      lowers to `_CG_prim_isinstance(x, t)` which the
#      C runtime doesn't define.  Only the
#      `isinstance(x, __pyc_None_type__)` path is wired
#      (lowered to NULL pointer check).
#
# Workaround used here: a single Expr class with a
# `kind` discriminator and `if/elif/elif` dispatch in a
# free function.  This is the C-style discriminated
# union pattern.  pyc handles it correctly.
#
# Filed as ifa/issues/029-polymorphic-dispatch.md.

class Expr:
  def __init__(self):
    self.kind = 0    # 0=const, 1=binop, 2=unaryop
    self.v = 0
    self.op = 0      # 0=add, 1=sub, 2=mul (binop) / 0=neg, 1=abs (unaryop)
    self.lhs = None
    self.rhs = None

def make_const(v):
  e = Expr()
  e.kind = 0
  e.v = v
  return e

def make_binop(op, lhs, rhs):
  e = Expr()
  e.kind = 1
  e.op = op
  e.lhs = lhs
  e.rhs = rhs
  return e

def make_unary(op, lhs):
  e = Expr()
  e.kind = 2
  e.op = op
  e.lhs = lhs
  return e

def evaluate(e):
  if e.kind == 0:
    return e.v
  if e.kind == 1:
    l = evaluate(e.lhs)
    r = evaluate(e.rhs)
    if e.op == 0:
      return l + r
    if e.op == 1:
      return l - r
    if e.op == 2:
      return l * r
    return 0
  if e.kind == 2:
    x = evaluate(e.lhs)
    if e.op == 0:
      return -x
    if x < 0:
      return -x
    return x
  return 0

# (1 + 2) * (3 - 4) = 3 * -1 = -3
e1 = make_binop(2, make_binop(0, make_const(1), make_const(2)),
                   make_binop(1, make_const(3), make_const(4)))
print(evaluate(e1))     # -3

# -((5 - 8) * 2) = -(-3 * 2) = 6
e2 = make_unary(0, make_binop(2, make_binop(1, make_const(5), make_const(8)),
                                  make_const(2)))
print(evaluate(e2))     # 6

# abs(2 - 9) = 7
e3 = make_unary(1, make_binop(1, make_const(2), make_const(9)))
print(evaluate(e3))     # 7

# Deeper: 1 + 2 + 3 + 4 + 5
e4 = make_binop(0, make_const(1),
                   make_binop(0, make_const(2),
                                  make_binop(0, make_const(3),
                                                 make_binop(0, make_const(4),
                                                                make_const(5)))))
print(evaluate(e4))     # 15

# 0 * (whatever) = 0  — tests short-circuit irrelevance
e5 = make_binop(2, make_const(0),
                   make_binop(0, make_const(99), make_const(100)))
print(evaluate(e5))     # 0
