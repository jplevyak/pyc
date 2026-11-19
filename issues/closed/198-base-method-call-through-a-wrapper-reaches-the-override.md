# 198 — `Base.m(self)` through a wrapper called the override

**Status:** closed 2026-11-19, the day it was filed. Found porting `logging`
([197](../197-logging.md)), where `StreamHandler.__init__(self, stream=None)`
calls `Handler.__init__(self)`, which relies on `level=NOTSET`. A wrong call
target. It showed as a compile error ("unresolved member", "no type") when
the override recursed into itself. With an override of compatible arity that
did not recurse, it would have run the wrong method silently.

## Symptom

```python
class B:
    def __init__(self, level=0):
        self.level = level
class C(B):
    def __init__(self, s=1):
        self.s = s
        B.__init__(self)      # relies on level=0
c = C()
print(c.level)
# pyc: unresolved member 'level' of class 'C'
```

Passing `level` explicitly (`B.__init__(self, 0)`) worked. So did a call
that needed no wrapper. Every wrapper kind failed the same way, and
`super().m()` with any of them:

| call | wrapper |
| --- | --- |
| `B.m(self, a)` relying on a default | `default_wrapper` |
| `B.m(self, b=x, a=y)` | `order_wrapper` |
| `B.m(self, 1, 2, 3)` into `*args` | `signature_wrapper` |
| `B.m(self, *t)` into named parameters | `unpack_wrapper` |

## Cause

`Base.m(recv, ...)` and `super().m(...)` dispatch statically. The receiver
moves into a temp whose `Sym::aspect` is the base class, and the matcher
dispatches as that class (`python_ifa_build_if1.cc`, the class-qualified
call; `pattern.cc` reads `a->var->sym->aspect`).

A call that needs adapting is routed to a wrapper (`python_ifa_sym.cc`). The
wrapper takes the actuals into fresh formals and forwards them in a new
send, which the matcher resolves from scratch. For a plain function the
callee slot is the function itself, so the new send can only reach it. For a
method the callee slot is the selector, and dispatch is on the receiver's
class. The wrapper's receiver formal had no aspect, so the forwarding send
dispatched on the receiver's own class and found its override. `IFA_DBG_FUNES`
showed it: the wrapper's out-edge went back into `C.__init__`.

## Fix

A wrapper exists for exactly one function (`Fun::wraps`). Its receiver
formal now carries the aspect of that method's class (`f->sym->self`'s
`must_specialize`), so the forwarding send can reach only the method it
wraps. This is the mechanism the original call used, in all four wrappers
(`pin_wrapped_receiver`). For an ordinary `x.m()` the target is unchanged:
dispatching as the wrapped method's own class finds that method.

The alternative, restricting a wrapper's forwarding send to `wraps` in the
matcher, would be generic ifa code for a frontend-built adapter, and wrappers
nest. The aspect is already how pyc says "dispatch as this class".

## Tests

`tests/base_method_call_through_wrapper.py`: all four wrapper kinds through
`B.m(self, ...)` with an overriding `C.m`, `B.__init__(self)` from a
subclass `__init__` that has a default of its own, and `super()` with a
default. Matches CPython on both backends.
