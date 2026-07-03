# Issue 009: Dict comprehensions silently drop their `for` clause

**Status:** open.
**Affects:** `python_ifa_build_if1.cc:1296-1317` (`PY_dict` case in
`build_if1_pyda`); `python.g:341-344` (`dictorsetmaker` grammar
rule, for the AST shape).
**Related:** issue 008 (set literals / genexpr / set
comprehensions — same comprehension-support gap, but that one
crashes the compiler outright; this one is worse in a different
way — it "succeeds" and bakes in a runtime abort).

## Symptom

A dict comprehension compiles with **exit code 0** and no fatal
error, but the generated C aborts the instant the binary runs:

```python
d = {x: x*2 for x in [1, 2, 3]}
print(d[2])
```

```
$ ./pyc dictcomp_test.py
warning: 'x' has no type
__pyc__:332: expression has no type
  called from __pyc__:604
dictcomp_test.py:1: unresolved call '__mul__'
dictcomp_test.py:1: illegal call argument type expression illegal: int64
dictcomp_test.py:2: expression has no type
$ echo $?
0
```

Generated C tail:

```c
t3 = _CG_f_3279_3/*__new__*/();
t2 = _CG_f_2053_0/*dict::__getitem__*/(/* d 3287 */ g4);
assert(!"runtime error: getter not resolved");
assert(!"runtime error: matching function not found");
```

The `for x in [1, 2, 3]` clause is never built at all — `x` is
referenced in the key/value expressions with no binding, which is
exactly what produces the "has no type" cascade.

## Root cause

`dictorsetmaker` (`python.g:341-344`) produces a `PY_dict` node
with **3 children** for a comprehension form: `[key_expr,
value_expr, comp_for_node]` — versus the flat, even-length
`[key, value, key, value, ...]` shape for a plain `{k: v, ...}`
dict literal (`python.g:345-347`).

`build_if1_pyda`'s `PY_dict` case assumes the flat shape
unconditionally:

```cpp
case PY_dict: {
  for (int i = 0; i + 1 < n->children.n; i += 2) {
    build_if1_pyda(n->children[i], ctx);
    build_if1_pyda(n->children[i + 1], ctx);
    ...
  }
  ...
}
```

For the comprehension shape (`n->children.n == 3`), this loop
runs exactly once (`i = 0`, since `1 < 3`), correctly building the
key/value pair as if it were a single-entry dict literal — then
`i` becomes `2`, and `i + 1 < n->children.n` is `3 < 3`, false, so
the loop stops. **The third child — the `comp_for` node driving
the whole comprehension — is never visited.** `x` is therefore
never bound as a loop variable anywhere, and the `dict()`
constructor is called exactly once instead of once per iteration.

## Proposed fix sketch

1. In the `PY_dict` case, branch on `n->children.n`: if it's
   exactly 3 and the third child's `kind == PY_comp_for` (matching
   the comprehension grammar shape), lower it like a list
   comprehension — build a loop over the `comp_for`'s iterable,
   binding its target var(s) each iteration, and inside the loop
   body call the key/value expressions then `__setitem__` on the
   accumulator dict. This likely means factoring out whatever
   loop-lowering logic already backs list comprehensions (working
   today per `tests/list_comprehension.py`) into a shared helper
   usable from both `PY_list`/`PY_testlist_comp` and `PY_dict`.
2. Apply the equivalent fix for `PY_set` when it carries a
   `comp_for` child (see issue 008) once that case gets real
   codegen instead of falling through to the default no-op.
3. Until the real fix lands, an interim `fail("dict comprehension
   not yet supported")` guard (checking for the 3-child /
   `PY_comp_for` shape) would at least convert this from "silently
   compiles a broken binary" to "clean rejection at compile time,"
   which is strictly better for anyone hitting this before the
   real feature is scheduled.

## Verification plan

1. `{x: x*2 for x in [1, 2, 3]}` produces a dict with entries
   `{1:2, 2:4, 3:6}`; `d[2] == 4`.
2. Comprehension with a filter clause: `{x: x for x in range(10)
   if x % 2 == 0}`.
3. No `assert(!"runtime error...")` in the generated C for any
   dict-comprehension test.
4. Add `tests/dict_comprehension_basic.py` + `.exec.check` — no
   dict-comprehension test exists today (confirmed: no test
   source matches `{.*:.*for.*in`).

## What this unblocks

Dict comprehensions are one of the most common idiomatic
constructs in modern Python (config/lookup-table building,
inverting mappings, filtering). The current failure mode — clean
compile, runtime abort — is particularly bad for anyone who
doesn't immediately execute the compiled binary in CI.
