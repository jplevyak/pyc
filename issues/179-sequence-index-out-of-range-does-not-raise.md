# 179 — an out-of-range sequence index reads memory instead of raising `IndexError`

**Status:** open. Found 2026-10-16 (the root of issues/178's defaultdict
symptom). Silent wrong answer, both backends, every mode.

## Symptom

```python
import sys
i = len(sys.argv)          # 1, so nothing is constant-folded
for name, seq in (("list", [1]), ("str", "a"), ("bytes", b"a")):
    try:
        print(name, repr(seq[i]))
    except IndexError:
        print(name, "IndexError")
```

| expression | CPython | pyc |
| --- | --- | --- |
| `[1][1]` | `IndexError` | `1` (whatever follows the element) |
| `"a"[1]` | `IndexError` | `'\x00'` (the NUL terminator) |
| `"a"[-2]` | `IndexError` | `'\x00'` |
| `b"a"[1]` | `IndexError` | `0` |

## Root cause (to confirm)

`list.__getitem__` and `tuple.__getitem__` are the `index_object` primitive
(`__pyc__/04_sequence.py`), and `str`/`bytes` indexing reads the buffer
directly. None of them compares the index with the length. A negative
index is normalized by adding the length, but the result is not checked
either.

## Fix

Bounds-check every integer index and raise CPython's `IndexError` (`list
index out of range`, `string index out of range`, …). A store
(`l[i] = v`) needs the same check, with `IndexError: list assignment index
out of range`. Points to settle:

- **Cost.** Indexing is the hot path of most corpus programs. Measure the
  check on the corpus before and after. Folding it where the analysis proves
  the index in range is the way to recover it, and a flag to omit it would
  be an accommodation (171).
- **Propagation.** A raise from an item subscript has no exception check
  after it today (issues/175's half), so confirm that `seq[i]` itself
  propagates the raise.

## Verification

`tests/sequence_index_out_of_range.py` (`.known_issue`) flips to PASS on
both backends.
