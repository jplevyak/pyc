# 114 — a generator can only carry integers; other yields come back as raw pointers

**Status:** open, found 2026-08-20 while clearing ifa/issues/090's
sunfish residue. **Silent wrong answer** — no diagnostic, plausible
output, wrong values.

## Symptom

```python
def gen():
    yield (1, 2)
    yield (3, 4)
for x in gen():
    print(x)
```

    CPython:  (1, 2)          pyc:  138797340475344
              (3, 4)                138797340475280

The numbers are the tuples' addresses. Nothing warns.

Comparing instead of printing does produce a diagnostic, but an opaque
one:

```python
for x in gen():
    print(x == (1, 2))      # warning: illegal primitive argument type 'x' illegal: tuple
```

## Cause

`__pyc_generator__` (`__pyc__/09_generator.py`) moves values through an
int-typed channel:

```python
nextval = 0
...
self.nextval = __pyc_c_call__(int, "_CG_generator_value", int, self.handle)
```

and the runtime matches — `long long _CG_generator_value(long long)`.
A machine word is fine for a pointer, so the DATA survives; what is
wrong is the declared TYPE. `nextval` is an `int`, so a yielded tuple
is an integer from FA's point of view onward.

## Why nothing caught it

Every generator test in the suite yields integers —
`generator_basic`, `generator_infinite`, `generator_return_value`,
`generator_yield_from`, `fibheap_full`, `cs_split_pools`. The
limitation has simply never been exercised.

## The missing conduit

`yield X` lowers (python_ifa_build_if1.cc, `PY_yield_expr`) to

    if1_send(..., sym_primitive, "yield", yval, yval_result)

and that send does **not** reference the `__pyc_generator__` instance
that will deliver the value. So there is no path for `yval`'s type to
reach the instance's `nextval` slot, and the hardcoded `int` in the
c_call is the only thing typing it.

A fix has to create that conduit: give the yield prim (or the
generator-construction lowering around
python_ifa_build_if1.cc:2500) a constraint flowing the yielded value's
type into the generator object's `nextval`, and stop hardcoding `int`
as the c_call's return type. FA already clones per contour, so once the
type is not pinned, each generator should specialise.

## Design traced 2026-08-20 — and it hits issues/018

A conduit does exist, end to end:

    yield X  ->  the generator function's `ret`
             ->  the wrapper's `handle_result`   (already the ctor arg)
             ->  __pyc_generator__.__init__      (add `self.nextval = handle`)
             ->  nextval

The wrapper already passes the call's result to the constructor
(`if1_add_send_arg(ctor_send, handle_result)`), so only the two ends
need work. **But it does not get there**, for two reasons, both
verified:

**1. `fn->ret` already carries an `int`.** `build_syms` gives every
generator a `_CG_generator_placeholder_return` typed `int` and moves it
into `fn->ret`. That placeholder is not incidental — its comment
explains it exists for a body that never falls through to the reply
(an unconditional `while True:` with no break), where FA would
otherwise flow no return type at all. So `nextval` would come out as
`{int, tuple}`, not `tuple`.

**2. `{int, tuple}` is unrepresentable.** Measured directly:

```python
class Box:
    def __init__(self, v): self.v = v
b = Box(0)
b.v = (1, 2)
print(b.v)          # pyc: assert(!"runtime error: matching function not found")
```

That is issues/018's family — a scalar unioned with a pointer, which
needs the tagged representation of
[ifa/030](../ifa/issues/030-DISPATCH-polymorphic-dispatch-fat-pointers.md).

**A third hazard**, if anyone reaches for the obvious shortcut: moving
the yielded value into `fn->ret` in LIVE code would clobber the
coroutine handle, because the generator function's real runtime return
value IS the handle. The conduit has to be type-only, and an IF1 move
is not.

### CORRECTION — this is NOT blocked on boxing

The paragraph that stood here concluded 114 needed `{int, pointer}`
unions solved (issues/018 / ifa/030). **That is wrong**, and shedskin
is the counter-example: it types sunfish — generators yielding move
tuples and all — with no boxing and no union representation at all.

How, from its source (`shedskin/cpp.py`, `generator_class`):

```cpp
class __gen_<funcname> : public __iter<YieldedType> {
```

It emits **one class per generator function**, parameterised on the
inferred yielded type (`nodetypestr(func.retnode.thing)`), with the
function's locals as concrete-typed members and an `int __last_yield`
for resumption. There is no shared generator object and no value
channel to widen, so no union ever forms.

Note especially what supplies the element type: **the generator
function's RETURN node**. In shedskin a generator's return type IS its
yielded type. That is exactly the conduit traced above — and shedskin
has no placeholder polluting it.

So the `{int, tuple}` union is not a fact about generators that pyc
must represent. It is manufactured by two pyc-specific choices:

1. every generator shares ONE `__pyc_generator__` whose value channel
   is declared `int`, and
2. `fn->ret` carries an `int` placeholder.

Specialise per generator and neither applies.

### Revised plan

Follow shedskin's shape within pyc's coroutine design (pyc uses real
coroutine handles, where shedskin uses a state machine — the handle
stays either way):

- Give each generator funcdef a synthetic variable **in the ENCLOSING
  scope**, written by every `yield` in its body. A dead store at
  runtime; its purpose is to collect the union of yielded types where
  something outside the generator can see it.
- The wrapper that constructs the generator object is built in the same
  `case PY_funcdef`, AFTER `gen_fun_pyda` — so that variable is in
  scope there. Pass it to the constructor as a type sample.
- `__init__` seeds `self.nextval` from the sample; `__pyc_advance__`
  takes its c_call result type from `self.nextval` rather than the
  hardcoded `int`. FA already clones `__pyc_generator__` per creation
  site, so each generator specialises — pyc's equivalent of shedskin's
  per-generator class.
- `nextval = 0` becomes `nextval = None`: `{None, T}` is representable
  for pointer-shaped T (verified), `{int, T}` is not.

Still not a small change, but it is frontend + library work of a kind
pyc already does, not a representational blocker.

## What this blocks

`shedskin_examples/sunfish` — its `gen_moves` yields move TUPLES, so
`move not in hist[-1].gen_moves()` (line 448) cannot work regardless of
the containment fix that landed alongside this filing. This is the
remaining half of issues/025 item 4.

More broadly: any generator over strings, tuples, lists or objects is
silently wrong today, which is most non-numeric generator code.

## Verification plan

- The repro above prints `(1, 2)` / `(3, 4)`.
- `x == (1, 2)` inside the loop compiles and matches CPython.
- A generator yielding strings round-trips.
- sunfish's line 448 no longer reports `unresolved call '__not__'`.
- Existing int-yielding generator tests unchanged.
