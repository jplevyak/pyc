# issues/161 — `random.seed(0)` and `seed(1)` were the same stream

**Status:** the collision is FIXED 2026-09-17. The Mersenne Twister gap below
is open.

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
