# 168 — an object at a computed `%s` prints raw memory

**Status:** open, root-caused (re-verified 2026-09-28: still prints raw bytes), reproducible on both backends. Found while
closing [165](closed/165-percent-d-truncates-a-64-bit-int-to-32-bits.md)'s
non-constant-format half, which fixed every other argument type at a
computed `%s` and left exactly this one.

**Related:**
[165](closed/165-percent-d-truncates-a-64-bit-int-to-32-bits.md) (the
mechanism this would extend — the per-argument type tag),
[040](closed/040-percent-format-float-arg-int-specifier-garbage.md) and
[123](closed/123-str-does-not-fall-back-to-repr.md) (the other `__str__` gaps).

## Symptom

```python
class P:
    def __str__(self):
        return "P!"

def fmt(k):
    return "%s" if k == 0 else "[%s]"

print(fmt(0) % P())     # CPython: P!
print("%s" % P())       # CPython: P!   -- and pyc gets this one right
```

| form | CPython | pyc (C and LLVM) |
| --- | --- | --- |
| `"%s" % P()` — constant format | `P!` | `P!` |
| `fmt(0) % P()` — computed format | `P!` | `\xef\xbf\xbd1o\xef\xbf\xbd\xef\xbf\xbdZ` |

**Compiles clean, exit 0, wrong output** — the class
[ifa/158](../ifa/issues/closed/158-FA-every-type-violation-is-fatal.md) exists to
stop and cannot catch, because nothing here fails to type. The bytes are
the object's own memory read as a NUL-terminated C string, so the output
is whatever happens to follow the struct; it changes between runs.

## Root cause

`%s` on an object needs `__str__`, and `__str__` is a **method dispatch** —
it has to run generated code. pyc does that in the FRONTEND
(`python_ifa_build_if1.cc:3844`): for a `fmt % args` whose format is a
compile-time constant, it walks the format, finds which arguments meet
`%s`, and inserts a `__str__` call on each before the
`__pyc_format_string__` send.

That walk needs the format. With a computed format there is nothing to
walk, so no argument is pre-converted, and the object pointer reaches
`_CG_format_string_tagged` as a bare pointer with tag `'s'` — identical to
a real string.

**Telling them apart would not help.** 165's type tag could carry an `'o'`
for "object", but the runtime still could not act on it: converting an
object to its `str` means calling back into generated code, and a C
runtime helper cannot dispatch a Python method. The repair has to happen
where the call can be emitted.

## Proposed fix

Pre-convert in the frontend **without** consulting the format: when the
format is not a compile-time constant, wrap every argument whose static
type is a user object (not numeric, not `str`) in a `__str__` call.

This is sound because an object has no other conversion it can meet.
CPython raises `TypeError: %d format: a real number is required` for
`"%d" % P()`; pyc does not model that, so converting unconditionally
changes nothing that works today. The numeric and string cases stay on
165's tag path, which already matches CPython for all of them.

Cost: one `__str__` call per object argument in a computed-format
expression, which is what the constant path already pays.

## Verification plan

- Extend `tests/format_nonconstant_string.py`: an object at a computed
  `%s`, an object at a computed `%s` with width (`"%10s"`), two objects in
  one computed format, and an object whose `__str__` itself formats.
- Both backends byte-identical to CPython, which is how the other 24
  cases in that file are pinned.
- `make test` green; no corpus program uses a computed format with an
  object argument today (`grep` finds none), so the sweep should be
  unchanged — confirm rather than assume.

## What this unblocks

Nothing in the corpus today, and that is worth stating plainly: this is
filed because it is a **silent wrong answer** that compiles clean, not
because something is waiting on it. It is the last piece of 165 —
everything else reaching a computed format now matches CPython — and a
program that hits it gets unpredictable output rather than a diagnostic.
