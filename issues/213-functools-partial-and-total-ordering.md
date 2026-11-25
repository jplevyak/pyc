# 213 — `functools.partial` and `functools.total_ordering`

**Status:** closed 2026-11-24 on landing, filed and done in one change.
What is left is `partial` with keyword arguments, which is
[189](189-unpacking-into-named-parameters.md) step 2.

## Symptom

Both were absent from `pyc_lib/functools.py`, so a program importing either
failed at the import. They were the last two common names of the module
(the website's WANTED list).

## partial

`pyc_lib/functools.py`: a class with `func`, `args` and `keywords` (always
empty). Its `__call__(*args)` is CPython's `func(*self.args, *args)`, which
the grammar has no form for (one star per call, issues/182). It is
`self.args.__pyc_call_prepended__(self.func, args)`, a tuple method
generated per fixed arity k of the stored tuple (python_ifa_main.cc,
`emit_tuple_arity_methods`):

```python
def __pyc_call_prepended__(self, f, args):    # self: arity k
  return f(self[0], ..., self[k-1], *args)
```

The stored elements keep their own types, and `*args` binds by its arity
(189 step 1), so the call binds named parameters, defaults and `*args`. A
runtime-length tuple, which none built by `partial(...)` is, concatenates.
The overloads are emitted only when a module calls `__pyc_call_prepended__`,
that is, when functools is imported.

Two designs were measured and dropped first (negative results):

- **`func(*(self.args + args))` with a fixed-arity `tuple.__add__`**, one
  overload per PAIR of arities. It works, and `print(t + u)` of
  heterogeneous tuples with it. But it is quadratic, and an overload costs
  analysis time where it is never called: on `itertools_more` the 49 pair
  overloads took FA from 7.4 s to 9.2 s, with no contour changed and none
  reached. On `__add__` itself they were candidates of every `+`
  (`collections_more` 46 passes to 54). And a concatenation's arity needs
  the per-arity methods to be printed. Adding every pairwise sum doubled
  the arity set where a `*args` call puts 0..N in it (`logging_basic` 13
  arities to 26, frontend 1.1 GiB to 5.4), and four suite tests hit the
  60 s compile cap.
- **A linear `__pyc_concat__`** (`__pyc_tuple_of__(self[0], ..., *t)` per
  arity of `self`, with `t` any fixed-arity tuple). With it,
  `itertools_more`'s flow analysis never finished pass 1, though no
  function it added was reached in the shorter prefix that still finished.
  The hang needs `zip_longest` plus several other lines of the test, and was
  not reduced further. Like [212](212-flow-analysis-never-finishes-a-pass-on-fysphun.md),
  it is an analysis non-termination worth a reduction if it reappears.

Unreached overloads costing time is itself a finding: `serial_pty`, whose
tuple arities reach 60, went from FA 5.9 s to 12.2 s with the per-arity
`__pyc_call_prepended__` it never calls, which is why they are gated.

**Keywords are refused.** `partial(f, k=v)` needs `**` into named
parameters, and a stored `keywords` dict of mixed values, both 189 step 2.
The frontend refuses a `partial(...)` call with a keyword argument, the
name resolved through the module's imports (python_ifa_dataclass.cc):

```
functools.partial with keyword arguments is not supported: binding them needs '**' into named parameters (issues/189)
```

Calling a partial with keyword arguments is refused as an unexpected
keyword of `__call__`.

## total_ordering

A class decorator adds methods, which a compiled class cannot get after its
definition, so it is lowered on the AST before build_syms, as `@dataclass`
is (python_ifa_dataclass.cc, `lower_total_ordering`). For a module-level
class decorated `@total_ordering` or `@functools.total_ordering`:

- The root is CPython's: the greatest name, as a string, of `__lt__ __le__
  __gt__ __ge__` that the class defines or inherits from a base defined
  earlier in the module.
- Each of the other three that the class does not have is generated with
  CPython's formula (`functools._convert`), for instance `__gt__` from
  `__lt__` is `not r and self != other`.
- CPython's `if op_result is NotImplemented` step is left out: pyc has no
  `NotImplemented` value, so a root cannot return one.

Refused, with CPython's message where it has one:

- no ordering operation: `must define at least one ordering operation: < > <= >=`;
- a base class from another module, which may define an operation the
  lowering cannot see;
- any other use of the name (a nested class, a call).

## Verification

- `tests/functools_partial.py`: positional, defaults, `*args`, a bound
  method, a lambda, nested partials, `map` and `key=`, a list of partials.
- `tests/functools_total_ordering.py`: each root, two roots, an inherited
  root, a subclass, sorting, `min` and `max`.
- Three `_refused` tests.

Both are identical to CPython on the C and LLVM backends and under
`--strict`. `make test`: every gate passes except two compile timeouts
(`collections_more`, `itertools_more`, both backends). They ran while
outside load on the machine was at 27-38; both pass alone, neither imports
functools (so their generated code is HEAD's), and side by side under
that load they compiled in the same time as HEAD's binary (14.7 s / 14.8 s).

Corpus: `check__default__6b970e6b+c656d7e3` has every verdict of
`check__default__bbce7f9f+b11efc73` (the sweep of the tree before). The
only difference is chaos and pisang at +1 EntrySet and +7 CreationSets,
both of which import functools (for `reduce`) and so now load `partial`.

Printing `p.keywords` (an empty dict nothing writes) can be refused,
depending on the rest of the program. That is ifa/072
(`dict_empty_next_to_populated`): the empty dict's `__str__` loop is typed
as live over a bottom element. It happens at HEAD without partial too.
