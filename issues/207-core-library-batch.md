# 207 — the core library batch: itertools, functools, collections, math, bisect, operator, time, random, string

**Status:** open for its residuals. Landed 2026-11-20. The modules pyc
already had were mostly stubs (`itertools` had `count` and `product`,
`functools` only `reduce`), and these are the ones most programs touch.
Every addition matches CPython 3.12 on both backends and under `--strict`.

## What is there

- **itertools**: `chain` (and `chain.from_iterable`), `islice`, `repeat`,
  `cycle`, `accumulate`, `groupby` (one shared lookahead; a group ends when
  the outer iterator moves on, as the C implementation's does),
  `zip_longest`, `pairwise`, `batched`, `compress`, `dropwhile`,
  `takewhile`, `filterfalse`, `starmap`, `permutations`, `combinations`,
  `combinations_with_replacement`. Each is a generator that consumes its
  input exactly as CPython's documented equivalent does; `islice` and
  `batched` check their arguments when called.
- **functools**: `lru_cache` (bare, `()`, `maxsize=N`, `None`, with
  `cache_info` and `cache_clear` and CPython's eviction order), `cache`,
  `wraps` and `update_wrapper` (the wrapper unchanged: pyc functions have
  no `__name__` to copy), `cmp_to_key`.
- **collections**: `Counter` (arithmetic, in-place operators, `most_common`
  in CPython's order, 3.10's equality), `OrderedDict` (`move_to_end`,
  `popitem(last=)`, order-aware `==`), and `deque`'s `maxlen`, `rotate`,
  `extendleft`, `insert`, `remove`, `count`, `index`, `reverse`, `copy`,
  item assignment and deletion, and repr. `defaultdict` gains
  `default_factory`, `setdefault`, `pop`, `update`, `copy`, `clear`.
- **math**: `gcd`, `lcm`, `factorial`, `comb`, `perm`, `isqrt`, `prod`,
  `isclose`, `isnan`/`isinf`/`isfinite`, `trunc`, `copysign`, `frexp`,
  `modf`, `ldexp`, `log1p`, `expm1`, `erf`, `erfc`, `gamma` and `lgamma`
  (CPython's own Lanczos code, not libm's), `cbrt`, `exp2`, the inverse
  hyperbolics, `hypot` of any number of coordinates, `dist`, `fsum`
  (CPython's exact algorithm), `sumprod`, `remainder`, `nextafter`, `ulp`,
  `nan`. **Every function now raises CPython's errors**: `sqrt(-1)`,
  `log(0)`, `acos(2)` are ValueError, `exp(1000)` OverflowError,
  `floor(inf)` OverflowError. They returned nan or inf, or for floor an
  arbitrary integer.
- **bisect** (new): `bisect_left`/`bisect_right`/`insort_*` with `key=`.
- **operator** (new): the operator functions and `itemgetter`.
- **time**: `perf_counter`, `monotonic`, `process_time` and their `_ns`
  forms, `time_ns`, `struct_time`, `gmtime`, `localtime`, `mktime`,
  `strftime` of a struct_time or 9-tuple (CPython's range checks),
  `asctime`, `ctime`.
- **random**: `choices` (weights and cum_weights), `gauss` (with its saved
  second value), `normalvariate`, `expovariate`, `lognormvariate`,
  `randrange` with a step and CPython's errors, `randbytes`. A seeded run
  draws CPython's exact values.
- **string**: `Template` (`substitute`, `safe_substitute`,
  `get_identifiers`, `is_valid`, CPython's invalid-placeholder message).

## Found and fixed on the way

Compiler:

- **A lambda anywhere inside a comprehension crashed flow analysis**
  (`make_AVar`, a display index out of range). A function's nesting depth
  counted the comprehension's scope, which pyc lowers inline, so the
  lambda's depth was one more than its display. `PycScope::inline_scope`
  marks comprehension scopes and the depth skips them.
- **A keyword argument in a decorator call asserted** (`@lru_cache(maxsize=None)`):
  decorator arguments were all added as positionals.
- **Calling an object whose `__call__` takes `*args`, defaults,
  keyword-only or `**kw` parameters failed to resolve**: the synthesized
  forwarder had the formal count and nothing else. It now mirrors the
  `__new__` wrapper (named formals, packed `*`/`**`, `__call__`'s
  defaults).
- **A raise inside a generator consumed by `list()` or `tuple()` escaped
  the handler** (the items before the raise were returned, and the
  exception was reported unhandled later): those intercepts now get the
  exception check a call gets.
- **`list(it)` of a list, tuple, range, str or slice iterator** was an
  unresolved member.
- **The LLVM backend's float `!=` was the ORDERED compare**, so `x != x`
  was false for a NaN (C's, and Python's, is true). `isnan` and every NaN
  test differed between the backends.

- **A `__pyc_c_call__` counted as a call's arguments when sizing the
  tuple methods.** Once a program has any `*args` function, the largest
  call's argument count sets the highest tuple arity the generated tuple
  methods cover, and a C call passes two values per argument: the new
  13-argument `time.strftime` made it 28, and `import logging` took 5 s
  of frontend instead of under 1 (all four logging tests timed out in
  `make test`). The frontend intrinsics are now left out of that scan.
  Still open: the generated source's parse cost grows steeply with arity,
  so a genuine call of about 13 or more arguments in a program that also
  has a `*args` function will be slow to compile (an unused 13-argument
  call measured 52 s of frontend and 177 s of if1-finalize).

Builtins:

- **`reversed(x)` ignored `__reversed__`** and indexed `0..len-1`, so
  `reversed(some_dict)` read the dict with integer keys. It dispatches
  `__pyc_reversed__`: a class defining `__reversed__` gets a synthesized
  bridge, dict reverses its keys, and the sequence protocol is the
  default.
- **`abs()` ignored `__abs__`** and returned `-0.0` for `-0.0`. int, float
  and bool define `__abs__` and `abs` dispatches.

## Not done, and why

| what | why | where |
| --- | --- | --- |
| `functools.partial` | forwarding its stored arguments needs `f(*a, *b)` (PEP 448, not in pyc's grammar) or `f(*(a + b))`, whose concatenated tuple has no compile-time length (189) | |
| `functools.total_ordering` | it adds methods to a class at run time | |
| `collections.namedtuple` | a tuple subclass, as `typing.NamedTuple` | [196](196-compiling-shedskin-with-pyc.md) item 4 |
| `operator.attrgetter`, `methodcaller` | an attribute read by a run-time name | 196 item 2 |
| `defaultdict` repr | it prints the factory's repr (`<class 'list'>`), which pyc cannot produce; refused with NotImplementedError | |
| `isinstance(Counter(), dict)`, `isinstance(OrderedDict(), dict)` | False here: they wrap a dict, as pyc cannot subclass one | |
| results past 64 bits (`factorial(21)`, `comb(70, 35)`) | wrap, as all pyc int arithmetic does | [177](177-int64-overflow-at-run-time-is-silent.md) |

## Gaps found, not fixed

- **`nonlocal`** is not supported (closed/001 left mutable captures out of
  scope): a nested function assigning an enclosing local is a compile error.
- **A builtin class as a value** (`f = list; f(x)`) is refused, and
  `x is list` emits invalid C (a C compile failure, not a wrong answer).
- **A generator that never yields a typed value** in some contour (it
  iterates an empty literal, or raises before yielding) leaves its value
  type empty: "unresolved member 'nextval'". The empty-literal family of
  [ifa/072](../ifa/issues/072-FA-empty-container-notype-current-mechanism-and-plan.md).
- **A module-level variable reused with another type** (`for f in names:`
  then `f = open(...)`) is a union pyc refuses; CPython rebinds.
- `string`'s Python 2 names (`string.split`, `join`, `letters`,
  `lowercase`, `uppercase`) are still provided, so a program using them
  compiles where CPython raises AttributeError, and `string.split`
  ignores `maxsplit`. An unflagged accommodation under rule 2.

## Tests

`itertools_more`, `functools_cache`, `collections_more`, `math_more`,
`bisect_operator`, `time_more` (independent of the time zone),
`random_more`, `string_template`, `reversed_protocol`, `abs_dispatch`,
`comprehension_lambda`, `decorator_keyword_args`, `call_object_signatures`,
`generator_raise_through_list`, `list_of_iterator`, `float_nan_compare`:
both backends and `--strict`.

## Verification (2026-11-21)

`make test` green after `make clean` (e2e 581 passed, 0 failed, 18 known,
on both backends). Corpus `check` sweep `check__default__f430bd66+8a134d9c`
against the previous one, `check__default__020ede00+4ce3adec`: no program
changed verdict. compile_fail 0, run_fail 0, and stdout_differs 1
(`othello2`, which differed before this batch too). CreationSets went from
2791 to 3025 as shapes went from 812 to 872 with the larger library, so the
count per shape moved only from 3.44 to 3.47.
