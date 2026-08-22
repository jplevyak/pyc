# 114 — a generator can only carry integers; other yields come back as raw pointers

**Status:** FIXED 2026-08-21 (see the FIXED section below). Found
2026-08-20 while clearing ifa/issues/090's sunfish residue. Was a
**silent wrong answer** — no diagnostic, plausible output, wrong
values. The long middle of this file is the investigation as it ran,
including two readings the FIXED section retracts; read that section
first.

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


## Implementation attempted 2026-08-20 — works, blocked on ONE conflict

The revised plan was built end to end and **it does fix the bug**:

    for x in gen():   # yields (1, 2), (3, 4)
        print(x)      # (1, 2) / (3, 4)   -- was two raw pointers

as did `x in gen()` over tuples and the sunfish-shaped
`while move not in gen():` loop. Suites went 286 -> 287 on both
backends with it on. All of it is REVERTED; this records what worked
and the one thing that does not.

### What the change was

1. **Frontend conduit.** `PycCompiler::gen_yield_sample` maps each
   generator function Sym to a synthetic Sym; every `yield` — and
   every `yield from`, which re-yields the sub-generator's values —
   also `if1_move`s the yielded value into it. Dead at runtime; only
   the type matters.
2. **Wrapper.** The `__pyc_generator__` construction (same
   `case PY_funcdef`, right after `gen_fun_pyda` builds the body)
   passes that Sym as a third constructor argument.
3. **Library.** `__init__(self, handle, ysample)` seeds
   `self.nextval = ysample`; `__pyc_advance__`/`send` take their
   c_call result type from `self.nextval` instead of a hardcoded
   `int`; the `nextval = 0` class attribute is deleted entirely.
4. **`c_call_codegen`** casts the result to the DECLARED type — the
   runtime returns a machine word while the declared type is now the
   yielded type, and the word IS the pointer.

Two things had to be found along the way, both real:

- **`yield from` needs the same hook.** A generator whose only yields
  are delegated (`def outer(): yield from inner()`) collects no sample
  and its channel ends up untyped. `generator_yield_from` caught it.
- **`return 0` on the exception paths of `__next__`/`send` pins the
  channel to int** all by itself — the union at the `for x in gen()`
  binding was `{int64, tuple}`, not `{None, tuple}`. Returning
  `self.nextval` instead keeps the type clean. This one is worth
  keeping in mind independently: an int literal on a
  never-observed path still widens the type.

### The blocker: visibility vs widening

The sample Sym must be at **module level** for the wrapper to reach
it. But writing yielded values into a global cell widens unrelated
types: `shedskin_examples/sudoku5`, whose generators yield LISTS,
stops compiling — its `[5, 3, 0, ...]` literal comes out with a
`_CG_void` element (`_CG_prim_list(_CG_void, 9)`). Isolated by
toggling the sample alone: sample off, sudoku5 compiles; sample on, it
does not.

Making the Sym an ordinary non-global instead widens nothing and is
not visible to the wrapper either — every generator then reports
"expression has no type".

**That conflict is the whole remaining problem.** It also cannot be
hidden behind a flag: the library half (dropping `nextval = 0`, typing
the c_calls from `self.nextval`) is unconditional, so with the conduit
disabled every generator reads `None`.

### Where to resume

Find a conduit that is visible to the wrapper without being a written
global. Options not yet tried:

- mark the sample so codegen/FA treat it as type-only (the `is_fake`
  flag already used for `__pyc_c_call__`'s type argument is the
  nearest existing precedent);
- attach the sample to the generator function's Sym rather than to a
  variable, and have the wrapper read it from there;
- or route the type through `fn->ret` after all, which is what
  shedskin uses — but that first needs the `int` placeholder
  (`_CG_generator_placeholder_return`) removed for generators that
  actually yield, since otherwise the union is `{int, T}` again.

### sunfish

Even with the conduit on, `sunfish.py:448` still reports
`unresolved call '__not__'`. Its `move` is `{None, tuple}` and the
comparison inside `__contains__` needs the None arm narrowed away,
which pyc does not do (ifa/issues/025, intra-function union
narrowing). So sunfish needs BOTH this and that.


## The fn->ret route also fails, for a different reason (2026-08-20)

Tried, since it is what shedskin does. **It cannot work as the
placeholder is currently arranged**, and the reason is worth recording
because it is not obvious.

The idea: stop typing the generator's placeholder return as `int` and
type it from a per-function Sym that every `yield` writes. `fn->ret`
then carries the yielded types, the wrapper's `handle_result` carries
them out to the constructor, and — the attractive part — **the Sym
never leaves the generator's own scope**, so none of the global-cell
widening from the previous attempt applies. No extra constructor
argument is needed either: `handle_result`'s runtime value is still
the coroutine handle, only its type changes.

It fails on ORDER. `build_syms` deliberately emits the placeholder
**before** the user's body — its comment explains why, and the reason
is real:

> A generator body whose own control flow never falls through
> (`while True: yield i`, no `break`) has no reachable path to whatever
> comes after the body; appending this there (as it used to) made the
> placeholder move dead code, and FA infers fn->ret as bottom/NOTYPE
> with no live move reaching it.

But the sample is only written by the yields, which are *in* the body.
So the placeholder reads it before any def exists: the Sym has no type
at that point, `fn->ret` ends up untyped, and every generator fails
with a cascade starting at

    warning: illegal call argument type '__pyc_generator__' illegal: __pyc_generator__

— including generators that only yield ints, which worked before.

Reverted. Suites back to 287 / 0 on both backends.

### What this leaves

Both routes are now understood and both are blocked by something
specific rather than by the union representation:

| route | blocker |
|---|---|
| module-level sample Sym | writing yielded values into a global cell widens unrelated types (sudoku5) |
| generator's own `fn->ret` | placeholder must precede the body; sample is defined inside it (use-before-def) |

The second looks the more tractable of the two. It needs the
placeholder's *reachability* and its *type* separated — e.g. keep an
unconditional early move into `fn->ret` for the dead-reply case, but
take the declared type from a construct emitted after the body, or
give FA the yielded type by a route that is not a data-flow read of a
local. The `is_fake` marker on `__pyc_c_call__`'s type argument is the
existing precedent for "this operand is consulted for its type, not
its value" and is the first thing to look at.


## The fn->ret route, done properly — a complete recipe, one item short

The earlier "fails on ORDER" verdict was wrong, and so was the
reachability reasoning behind it. Re-derived from the code:

`fa.cc`'s `P_prim_reply` flows a Fun's return type **at the reply
node**, from `make_AVar(reply operand)` into `es->rets`, and
`add_pnode_constraints` only visits LIVE pnodes. So there are two
independent requirements, which `build_syms` meets with two separate
devices:

- **the reply must be live** — an opaque never-taken branch to
  `label[0]`, conditioned on the placeholder call's value (a C call
  precisely so FA cannot fold the branch);
- **fn->ret must have a reaching def there** — the early
  `if1_move(default_ret → fn->ret)`.

The early move is needed for the VALUE, not for the type union.
`fn->ret`'s type is whatever flows in at **any** live point, so yields
can contribute — no ordering problem, no global cell, nothing that
widens unrelated types.

Built on that basis, five of six pieces work:

1. **`build_syms`**: move `sym_nil` into `fn->ret`, not `default_ret`.
   `default_ret` must stay opaque because it is the never-taken
   branch's condition; using it for the type is what pinned the
   channel to int. `{None, T}` is representable where `{int, T}` is
   not.
2. **`build_if1`**: at each `yield` (and each `yield from`, which
   re-yields), `if1_move(yval → ctx.fun()->ret)`. Free at runtime —
   a generator's real result is the coroutine handle, produced by the
   coroutine machinery, not by fn->ret.
3. **library**: drop `nextval = 0` entirely; take the c_calls' RESULT
   type from `self.handle` (which now carries `{None, yielded}`)
   rather than a hardcoded `int`; and replace `return 0` on the
   exception paths of `__next__`/`send`, which pins the channel to int
   by itself.
4. **`c_call_codegen`**: cast the result to the DECLARED type — the
   runtime returns a machine word, the declared type is now the
   yielded type, and the word IS the pointer.
5. **`c_call_codegen`**: cast arguments declared `int` to
   `long long` **for `_CG_generator_*` only**. Their handle argument
   is a plain machine word but now arrives union-typed. Deliberately
   not blanket: `list.__add__` declares `int` for a whole LIST because
   `_CG_list_add` converts internally, and casting there would destroy
   the pointer.

**The sixth**, which is where it stops: the generator function's **C
return type is hardcoded to the handle's `int64`**, while FA now types
the call's result as the yielded type —

    t2 = _CG_f_10602_21/*gen*/();     // t2 is _CG_ps11305*, callee returns int64
    error: incompatible integer to pointer conversion

so codegen's `is_generator` path has to emit the function with fn->ret's
type and `return (T)handle;`. That is the one remaining piece, in
`cg.cc`/`cg_emit_llvm.cc` rather than in the frontend.

All reverted; suites 287 / 0 on both backends. Nothing here is
blocked on representation — the recipe above is concrete and the
remaining item is a codegen signature change.

## All six pieces applied — and it is STILL not shippable

The sixth piece was written (`c_type(f->rets[0])` in
`write_c_fun_proto`, and `return (T)(uintptr_t)…handle.address()` in
the tail). With all six in, plus two more found along the way:

- **drop the `sym_nil` move into `fn->ret` entirely.** It was added to
  give the never-taken branch a reaching def, but AVars accumulate per
  (Var, contour) rather than per path, so the yields' own defs suffice
  — and moving nil put None back into the union, where a `{None, T}`
  RECEIVER cannot dispatch a method even though it prints fine
  (`None in gen()` aborted inside `__pyc_generator__::__contains__`).
  The reply's *liveness*, the thing that actually mattered, is handled
  by the opaque branch alone.
- **`P_prim_len` must not fold a CreationSet with no defs.** Such a CS
  was never built by a creation site — it is abstract, or synthesised
  for an opaque `__pyc_c_call__` result, which is exactly a
  generator's value channel. `vars.n` is 0 because nothing filled it,
  not because the container is empty.

That gets single-yield generators fully correct:

    for x in gen():  print(x)          (1, 2)        ✓
    len(x), x[0], x[1]                 2 1 2         ✓
    x == t                             True          ✓
    None in gen()                      False         ✓

### Where it breaks: a generator with TWO yields

    def gen():
        yield (1, 2)
        yield (3, 4)
    for x in gen():
        print(len(x), x[0], x[1])

    CPython:  2 1 2 / 2 3 4
    pyc:      2 3 4 / 2 3 4

Both iterations report the SECOND tuple. `x[0]`/`x[1]` fold to
compile-time constants drawn from one of the two CreationSets now
reaching the channel, so the values are plausible and wrong. Equality
goes the same way — `x == (1, 2)` is silently False for a tuple that
really is `(1, 2)`.

**This is a worse failure mode than the bug being fixed.** At baseline
that program aborts loudly ("primitive operand type mismatch"); with
the fix it prints confident nonsense. Measured by stashing, so it is
the change's doing, not a pre-existing condition.

Reverted for that reason alone — every other measurement was positive.

### What the next attempt has to solve first

Constant folding of indexing/arity across a MULTI-CreationSet channel.
Note the control: two tuple CreationSets reaching one variable through
an ordinary function return —

    def f(b):
        if b: return (1, 2)
        return (3, 4)
    print(f(1) == t, f(0) == t)        # True False, correct

— works fine. So the folding is not wrong about unions in general; it
is wrong about a union arriving through the synthesised CS of an
opaque c_call, which is the shape this fix creates. That is the thing
to fix before re-applying the six pieces, which are otherwise
believed correct and are recorded above in full.

## FIXED 2026-08-21

Both suites 287 passed / 0 failed, C and LLVM. Regression test:
`tests/generator_yields_nonint.py`.

```python
def gen():
    yield (1, 2)
    yield (3, 4)
for x in gen():
    print(x)              # (1, 2) / (3, 4)   -- was two raw addresses
```

Tuples, strings and lists all round-trip, `in` works over them,
multi-yield generators report each value in order, and a bare `return`
no longer aborts the program.

### Answering the question this issue stalled on

The last two sections above are both wrong about the cause, and the
control they lean on — two tuple CreationSets through an ordinary
`return` working fine — turned out to be a red herring twice over.
(`392155ad` on `main` summarised those same two sections as a "Branch
`issues/114-generator-typed-channel`" note reaching the same `_CG_void`
conclusion; it is retracted by point 2 below and was dropped when this
branch merged, rather than left standing alongside its own refutation.)

**There was never an asymmetry between a return and a reply.** Two
tuple CreationSets union at a reply exactly as well as at a return;
the emitted C for both is a single concrete record type. Three
separate things were being read as one:

1. **`fn->ret` is single-assignment-renamed.** Sequential `if1_move`s
   into it kill each other, so the reply saw only the LAST yield. That
   is what made both iterations report the second tuple — not folding
   "across a multi-CreationSet channel", because there was only ever
   one CreationSet to fold. Multiple `return` statements union because
   each reaches the reply on its own path and the join inserts a phi;
   two yields are straight-line and do not. Giving each yield its own
   never-taken path to the reply (`gen_yield_type_contribution`,
   python_ifa_build_if1.cc) makes the union form.

2. **The "collapses to `_CG_void`" reading was a no-op.** The helper
   looked up the generator's opaque placeholder in a map filled by
   `gen_fun_pyda` — but build_if1 is a POST-ORDER walk, so every yield
   is visited before `gen_fun_pyda` ever runs on the enclosing
   `PY_funcdef`. It read an empty map and returned at every single
   yield. With no other def of `fn->ret` left, the return type had
   ZERO reaching defs: bottom, printed as "expression has no type" and
   emitted as `_CG_void_type`. That is not a union collapsing, it is
   nothing arriving. Each yield now builds its own placeholder.

3. **Constant folding was real, but somewhere else entirely.** Not
   indexing across a channel — a generator whose body yields ONE
   constant (`yield 1`, then a raise or a fall-through) makes FA
   certain the FUNCTION returns 1. A generator's C return value is the
   coroutine handle, though: cg.cc discards the emitted return and
   hands back `handle.address()`. So the caller inlined the literal in
   place of the handle, built `__pyc_generator__` around the address 1,
   and segfaulted on the first resume — `_CG_generator_send(raw_handle=1)`.
   Fixed in `P_prim_reply` (ifa/analysis/fa.cc) by unioning a
   constant's own abstract type alongside it for a generator, so
   `get_constant` sees two CreationSets and refuses to fold. Done at
   the type rather than at the fold because SSU gives the call's result
   and each later use separate Vars AND separate Syms — there is no
   single downstream thing to exempt. Same root as issues/022's
   `P_prim_await` liveness exception: **a coroutine handle is not a
   value the optimizer may reason about through its contents.**

### What landed

Beyond the six pieces recorded above (`gen_placeholder` is gone, per
point 2):

- `gen_yield_type_contribution` (python_ifa_build_if1.cc) — one
  never-taken branch per yield, each moving its value into `fn->ret`
  and jumping to the reply, conditioned on its own opaque
  `_CG_generator_placeholder_return()` so FA cannot fold either arm.
- **`fn->ret` no longer does two jobs.** Its runtime value is the
  handle; its type is the yielded union. The wrapper now routes the
  handle through `_CG_generator_handle` (an identity added to
  pyc_c_runtime.h and pyc_runtime.c) typed as a plain `int` meta type,
  and passes the call's own result on separately as a type sample:
  `__pyc_generator__(handle, sample)`, with `__init__` seeding
  `self.nextval = sample`. The opaque call alone was NOT enough —
  the constant was inlined at its ARGUMENT — which is why the fold
  also had to be stopped in FA.
- **A bare `return` in a generator contributes nothing to `fn->ret`.**
  It used to move `int64_constant(0)` "for consistency with
  gen_fun_pyda's int64-typed default reply", but that default reply is
  the very thing this issue removed; all it did was manufacture an int
  arm, and `{int, tuple}` aborts. Cost: StopIteration.value after a
  bare return is undefined rather than 0 (CPython reports None; pyc
  already diverged). Explicit `return X` is unaffected.
- The `P_prim_len` no-defs guard recorded above is still in and still
  correct.

### Not blocked by this: sunfish

`Position.gen_moves` is a **method**, and at the time this landed a
generator method never got the `__pyc_generator__` wrapper at all — see
[issues/115](115-generator-methods-unsupported.md), a pre-existing gap
that failed on plain ints and had nothing to do with the value channel.
Reduced both ways to be sure: the same program with a module-level
generator matched CPython exactly.

115 was fixed the same day, and `gen_moves` now compiles clean. sunfish
itself is not signed off here — see that issue's `### sunfish: this was
not the last blocker`.
