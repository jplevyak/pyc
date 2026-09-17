# issues/161 — `random.seed(0)` and `seed(1)` were the same stream

**Status:** FIXED 2026-09-17 — both the seed collision and the generator.
`pyc_lib/random.py` is now MT19937 and reproduces CPython's stream bit for bit.

## The collision

`pyc_lib/random.py`:

```python
def seed(a):
    _state = a & 0x7fffffff
    if _state == 0:
        _state = 1        # <-- seed(0) IS seed(1)
```

Measured on `random.seed(d); [random.randint(1,3) for _ in range(12)]`:

```
before   0 [2, 1, 1, 2, 3, 1, 3, 1, 2, 1, 1, 2]
         1 [2, 1, 1, 2, 3, 1, 3, 1, 2, 1, 1, 2]   <- identical
after    0 [1, 2, 1, 3, 1, 2, 2, 2, 2, 1, 2, 3]
         1 [2, 1, 1, 2, 3, 1, 3, 1, 2, 1, 1, 2]
```

The guard was also **unnecessary**: the LCG is
`_state = (_state * 1103515245 + 12345) & 0x7fffffff`, which has no fixed point
at zero (`0 -> 12345`). Removing it is the whole fix.

Found via `shedskin_examples/dijkstra2`, which does `random.seed(d)` for
`d in range(10)` and prints one result per seed. Exactly **25 of its 50 output
lines** were duplicates, because two of the ten seeds were the same stream.

## Open: it is an LCG, not CPython's Mersenne Twister

`random` in Python is a *specified* algorithm — MT19937 with `init_by_array`
seeding, `random()` as `genrand_res53`, and `randrange`/`randint` built on
`getrandbits` with rejection sampling. pyc's shim is a glibc-style LCG, so a
seeded program's stream differs from CPython's and its output cannot be
compared.

**Scope, measured:** 26 corpus programs import `random`; **23 of them seed
deterministically** — `amaze ant chaos chull dijkstra dijkstra2 genetic
genetic2 go kmeanspp mao mastermind2 neural2 oliva2 path_tracing pygmy pylife
quameon rdb rubik rubik2 timsort voronoi`. For all of these the corpus
`stdout_differs` verdict says nothing about pyc's correctness, the way
`sudoku5`'s `TIME` line did (ifa/158).

That makes MT19937 worth implementing on its own terms: it would turn a third
of the corpus from "incomparable" into "comparable", which is the same argument
ifa/158 made for making violations fatal. The file's own header already admits
the deviation ("does NOT reproduce CPython's Mersenne Twister sequence").

## FIXED: MT19937, matching CPython bit for bit

**Author: "plib includes the MT19937 source, can we copy that in?"**

**Not that file.** `plib/mt19937-64.cc` is the **64-bit** variant — `MATRIX_A
0xB5026F5AA96619E9`, `NN 312`, `MM 156`, multiplier `3935559000370003845`.
CPython's `random` is the **32-bit** one — `0x9908b0df`, `N 624`, `M 397`. They
are different generators with different streams, so copying it would have
replaced one non-CPython PRNG with another.

What CPython actually specifies, and what `pyc_lib/random.py` now implements:

- MT19937-32 core, seeded by `init_by_array` over the seed's absolute value as
  little-endian 32-bit words;
- `random()` = `genrand_res53` — `((a>>5)*67108864 + (b>>6)) / 2**53` over two
  draws;
- `getrandbits(k)`, and `_randbelow(n)` by rejection sampling on
  `bit_length(n)` bits;
- `randrange`/`randint`/`choice`/`shuffle` built on `_randbelow`, as CPython
  builds them.

**Verified against CPython, not assumed.** A reference implementation was
written and checked first, then ported; both match:

```
seed   pyc                                  CPython
0      0.84442185152504812 0.75795440294030247   identical
1      0.13436424411240122 0.84743373693723267   identical
2      0.95603427188924939 0.94782748705934938   identical
12345  0.41661987254534116 0.010169169457068361  identical
```

and the derived API is byte-identical too — `randint`, both `randrange` forms,
`choice`, `uniform`, `shuffle`, `getrandbits` all produce CPython's exact
sequences from the same seed.

`sample()` is the one deliberate exception: CPython switches between a
selection-set and a pool algorithm on a size heuristic, so its *order* differs.
It draws from the same generator, so a seeded run is deterministic; no corpus
program depends on its order.

### What it buys, and why the sweep does not show it

Corpus `-m check` is unchanged (35 / 12 / 19). That is not because nothing
improved — it is the same blind spot ifa/158 found in `sudoku5`'s `TIME` line.
Measured directly after the change:

| | diff vs CPython |
| --- | --- |
| `genetic` | **1 line — `TIME 22.00`** |
| `circle` | **1 line — `3.11.0 (pyc)`** |
| `voronoi` | 41 lines |
| `ant` | 401 lines |

`genetic` seeds deterministically and its entire content now matches CPython;
only the elapsed-time line differs, and the harness counts that as a difference.
So the sweep's `stdout_differs` is an over-count of unknown size, and the
`random` fix is worth more than the metric shows.
