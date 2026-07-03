# Issue 016: Five newer Python syntax forms aren't in the grammar at all

**Status:** open.
**Affects:** `python.g` (the DParser grammar — none of the five
forms below have a production rule).
**Related:** issues 006-014 (all of those are semantic/lowering
gaps for syntax the grammar *does* parse; this issue is the
opposite case — syntax the grammar doesn't recognize at all, so
these fail as parse errors rather than reaching the frontend).

## Why grouped together

Unlike issues 006-014, each of these five forms is a **pure parse
gap** — `python.g` simply has no production for them, so every one
fails identically at the DParser stage (`syntax error after
'TOKEN'`), before any frontend lowering code ever runs. They share
one root cause location (the grammar needs a new rule per form)
and, at least for a first pass, one reasonable disposition
(clean rejection is already what happens — the actionable work is
purely additive grammar+lowering, no existing behavior to
preserve). Filed together for discoverability; each sub-item below
can be split into its own issue once someone picks it up, since
their implementation effort and priority differ significantly.

## Sub-gap 1: `async`/`await` (PEP 492, Python 3.5+)

```python
async def foo():
    return 1
```
```
async_test.py:1: syntax error after 'async'
```
No `async`/`await` keyword or rule anywhere in `python.g`.
Implementing this meaningfully requires an event-loop/coroutine
runtime model — likely the largest sub-item here, probably larger
in scope than issue 014's generators (which it depends on/relates
to, since `async def` bodies are generator-like).

## Sub-gap 2: Walrus operator `:=` (PEP 572, Python 3.8+)

```python
if (n := 5) > 3:
    print(n)
```
```
walrus_test.py:1: syntax error after 'n'
```
No `:=` token in `python.g`. This is a small, self-contained
addition: extend the relevant expression rule (likely `test` or a
new `namedexpr_test` mirroring CPython's own grammar split) to
accept `NAME ':=' test`, and lower it to an assignment-then-use in
`build_if1_pyda`. Good candidate for a first, low-effort pass
among these five.

## Sub-gap 3: Structural pattern matching (`match`/`case`, PEP 634, Python 3.10+)

No grammar rule; `match`/`case` aren't even reserved words today
(they're soft keywords in real Python 3.10+, contextual on
appearing at statement-start before a `:`). This is a substantial
addition — full pattern matching (literal patterns, capture
patterns, class patterns with attribute binding, or-patterns,
guards) is comparable in scope to a small compiler feature of its
own; a minimal subset (literal + capture patterns only, no class
patterns) would be a reasonable first slice.

## Sub-gap 4: PEP 484 type annotations (`def f(x: int) -> int:`, `x: int = 0`)

```python
def f(x: int) -> int:
    return x + 1
```
```
annot_test.py:1: syntax error after 'x'
```
No `':' test` after a parameter name, and no `'->' test` after the
parameter list, in `funcdef`/`parameters` (`python.g:47` and
nearby). Also affects variable annotations (`x: int = 0`) in
`expr_stmt`. Since pyc's type system is inferred via FA rather than
declared, the pragmatic first slice here is almost pure
**parse-and-discard**: accept the annotation syntax, verify the
annotation expression parses (so e.g. `List[int]` doesn't need to
"work", just not be a syntax error), and drop it — matching how
CPython treats annotations as optional/advisory unless
`typing.get_type_hints` is used. This is likely the easiest
sub-item to land since it needs no new semantics, only new grammar
productions that are silently accepted.

## Sub-gap 5: Extended iterable unpacking (`a, *b = [1, 2, 3]`, PEP 3132, Python 3.0)

```python
a, *b = [1, 2, 3]
```
```
star_unpack.py:1: syntax error after ','
```
No starred-target form in the assignment-target grammar
(`expr_stmt`, `python.g:80`). Note this is *not* the same gap as
`*args` in function definitions/calls, which already works (see
`PY_star_arg`/`PY_dstar_arg` handling in
`python_ifa_build_syms.cc`/`python_ifa_build_if1.cc`) — this is
specifically the assignment-target position. Once parsed, the
lowering needs to bind the starred name to a (list) slice of the
remaining unpacked elements, alongside the existing plain
multi-target unpacking already supported (`tests/tuple_unpack.py`).

## Verification plan (per sub-gap; land and verify independently)

1. Walrus: `if (n := 5) > 3: print(n)` prints `5`.
2. Type annotations: `def f(x: int) -> int: return x + 1` compiles
   and runs identically to the unannotated form; `x: int = 0` at
   module/function scope parses.
3. Extended unpacking: `a, *b = [1, 2, 3]` gives `a == 1`, `b ==
   [2, 3]`; also the leading/middle-star forms (`*a, b = ...`,
   `a, *b, c = ...`).
4. match/case: minimal literal+capture pattern test.
5. async/await: minimal `async def` + `await` round-trip (largest
   lift; may warrant its own design doc before implementation,
   similar to issue 014).
6. Add one test file per sub-gap once implemented — zero coverage
   for any of the five today (confirmed via grep across
   `tests/*.py`).

## What this unblocks

These are all standard modern-Python syntax that appears in
contemporary CPython code; annotations and the walrus operator in
particular are common enough in current Python style guides that
their absence is likely to surprise anyone porting recent code to
pyc, even though (unlike issues 006-010) at least the failure mode
here is an honest parse error rather than a silent miscompile.
