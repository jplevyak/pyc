# 042 — no support for package-directory imports (`pkg/__init__.py` + submodules)

**Status:** open, filed 2026-08-08. Confirmed still real —
[issues/025](025-shedskin-examples-coverage.md)'s TODO list item 15
had already named this precisely ("the last structural import
blocker") but it was never turned into its own issue file, despite
the doc's own text saying "package/multi-file layouts... need
package-directory import resolution" back on 2026-07-22.

**Affects:** `python_ifa_build_syms.cc`'s `build_import_syms` and
`python_ifa_build_if1.cc`'s `build_import_if1` — both search for a
module by treating its dotted name as a single flat filename
(`<mod>.py`) on the search path; neither has any notion of a
directory-based Python package.

## Confirmed real, both example shapes

```
$ pyc minilight.py
fail: error line 8, cannot find module 'ml' (no 'ml.py' on the search
path; pyc does not yet provide this module)
```
`minilight.py` does `from ml import entry`, where `ml/` is a real
directory (`shedskin_examples/minilight/ml/`) containing
`__init__.py`, `entry.py`, `camera.py`, `scene.py`, etc.

```
$ pyc tarsalzp.py
fail: error line 32, cannot find module 'com.github.tarsa.tarsalzp.Main'
(no 'com.github.tarsa.tarsalzp.Main.py' on the search path; pyc does
not yet provide this module)
```
`tarsalzp.py` imports a **four-level-deep** dotted package path
(`com/github/tarsa/tarsalzp/Main.py`), confirming this isn't just a
one-level gap — nested packages need to work too. `quameon` (not
individually re-tested here, same class per the original 2026-07-22
note: `jastrow/`, `observables/`, `orbital/`, `stats/` subpackages)
is very likely the same shape.

## Root cause

`build_import_syms` (`python_ifa_build_syms.cc:62`) resolves an
import by searching `ctx.search_path` for a file matching
`<mod>.py`, where `mod` is the full dotted import name treated as one
flat string (only literal dotted-fallback handling exists: `import
os.path` with no `os.path.py` on disk falls back to binding the
top-level `os` component — a deliberate, narrower accommodation for
stdlib shims like `os.py`'s `path = _os_path()` attribute, not real
package support). Notably, the code already has an explicit guard
against treating a package directory as an importable name at all:
```cpp
for (auto p : ctx.search_path->values()) {
  if (file_exists(p, "/__init__.py")) continue;  // skips package dirs
  if (!is_regular_file(p, "/", mod, ".py")) continue;
  import_file(mod, p, ctx);
  break;
}
```
This `if (file_exists(p, "/__init__.py")) continue;` line means: if
the search-path entry *itself* is a package root, it's skipped for
this lookup — there is no code path anywhere that looks for
`<search_path>/<mod>/__init__.py` or `<search_path>/<mod>/<submodule>
.py`. `build_import_if1` (`python_ifa_build_if1.cc:19`) mirrors the
same flat-filename assumption for the IF1-building pass. Both would
need the same fix, in the same shape.

## What a fix needs

Real Python package resolution: for a dotted import name
`a.b.c`, walk each search-path entry looking for `a/` (a directory
containing `__init__.py`); if found, treat it as a package whose own
content comes from `a/__init__.py`, then resolve `.b` within it by
looking for either `a/b.py` (a submodule file) or `a/b/__init__.py`
(a nested subpackage), recursively. This needs to compose with the
*existing*, already-working single-file module machinery
(`import_file`, `get_module`, `PycModule`) rather than replace it —
each resolved package/submodule is still just a `PycModule` once
found; what's missing is purely the *directory-and-`__init__.py`-aware
search* step before that machinery takes over.

Two real corpus shapes to keep in mind while designing this:
- `from ml import entry` (minilight) — importing a *submodule* by
  name via `from package import submodule`, not just an attribute
  defined in `__init__.py`. `build_import_syms`'s `from`/`sym`
  handling already distinguishes `from X import Y`
  (bind `Y`) from `import X` (bind `X`) at the single-file level; a
  package-aware version needs the same distinction, where `Y` might
  resolve to either an attribute of `X/__init__.py` **or** a sibling
  file `X/Y.py` — real Python tries the submodule-file case when the
  attribute doesn't already exist in the package namespace.
- `com.github.tarsa.tarsalzp.Main` (tarsalzp) — a purely nested,
  multi-level nesting with (seemingly) no `from`, i.e. `import
  com.github.tarsa.tarsalzp.Main` or similar — needs the walk to
  recurse through 4 directory levels, not just 1.

## Verification plan

- `minilight.py` (1-level package, `from pkg import submodule` shape)
  and `tarsalzp.py` (4-level nested dotted import) are real,
  already-vendored regression candidates — get both compiling and
  running, ideally byte/pixel-comparable to CPython the way this same
  audit did for `issues/040`'s yopyra investigation (minilight is
  itself a raytracer with a `.ppm`-shaped comparison available).
  `quameon` is a third, larger example worth a follow-up check once
  the mechanism works, not required to close this issue.
- Full `test_pyc.py` both backends — this touches core import
  resolution, exercised by every corpus example and presumably several
  `tests/*.py` fixtures already; treat any new failure as a signal to
  narrow the fix.
- Add a new, minimal `tests/` fixture with a real 2-level package
  directory (something like `tests/pkgdirs/mypkg/__init__.py` +
  `tests/pkgdirs/mypkg/sub.py` + a driver importing both styles) since
  nothing in the existing suite exercises this at all — confirmed via
  `grep -rl "^import\|^from" tests/*.py` finding no package-directory
  shape today.

## What this unblocks

`minilight`, `quameon`, and `tarsalzp` in the shedskin corpus — the
only remaining structural (not missing-feature, not FA-precision)
import blockers, per the corpus sweep's own 2026-07-22 note. Also the
general Python package feature itself, for any future program (corpus
or otherwise) organized as a real multi-file package rather than a
flat script.
