# 041 — stdlib shims must be real or refuse: `hashlib` is still a silent stub

**Status:** open, narrowed. Rewritten 2026-09-28; the per-module history
is in git: `git show e3b44e2c:issues/041-stdlib-shim-stubs-silently-wrong.md`.
`colorsys` (2026-08-08), `getopt`, `os`'s filesystem functions,
`os.path.isdir/exists/islink`, `sys.version`, `string` (2026-08-11) and
`struct` (integer codes, 2026-09-27, `tests/struct_pack_unpack.py`) are
all real implementations now.

## The rule

A shim in `pyc_lib/` either implements the function to CPython's
semantics or **raises / refuses** for what it does not implement. A stub
that returns a plausible value (`b""`, `()`, `0`, `""`) is a silent wrong
answer. That is worse than a missing module, because nothing in the
compile or run reports it.

## Open

- **`hashlib`**: `md5(...).hexdigest()` returns `""`. `rsync` will hit
  it once it compiles (issues/025 triage). Implement `md5`/`sha1` (or
  whatever the corpus uses: `rsync`, `sha`), or make the stub raise
  `NotImplementedError`.
- **`struct` gaps:** float codes (`f`, `d`), `s`/`p` and `?` raise, which
  is correct. `msp_ss` needs `'>H8xBB4x'`, whose pad bytes are already
  supported, and `b'%c' % int`.
- **`minpng`**: `struct.pack('<BHH', bool(last), n, 0xffff ^ n)` builds
  the rest tuple `(bool, int, int)` read at a runtime index, and the
  1-byte/8-byte mix has no representation. Widening `bool` to `int` in
  the record would pack correctly but print `1` for `True`: an
  accommodation, so permissive-only under
  [171](171-permissive-accommodations-must-be-flagged-and-non-strict.md).
  The alternative is a corpus edit, `int(bool(last))`, which is a CPython
  no-op for `pack`. Prefer the edit (PYC_CHANGES.md: no accommodation
  where a no-op edit states the intent).

## Audit to repeat

Grep `pyc_lib/*.py` for functions whose body returns a constant, and for
`# stub` comments. Each hit is either implemented or made to raise.

## Verification

Each implemented function matches CPython byte-for-byte in a `tests/`
fixture on both backends. Each unimplemented one raises, with a test
showing that it does.
