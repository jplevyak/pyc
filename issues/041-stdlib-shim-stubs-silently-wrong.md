# 041 — stdlib shims must be real or refuse

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

- **`hashlib`** (FIXED 2026-10-07): `md5` is a real RFC 1321
  implementation (`tests/hashlib_md5.py`), which is what rsync uses. The
  other algorithms raise `NotImplementedError` rather than returning an
  empty digest.
- **`struct` gaps:** float codes (`f`, `d`), `s`/`p` and `?` raise, which
  is correct. (`minpng`'s `pack('<BHH', bool(last), ...)` is fixed, with
  no corpus edit: `pack` reads its `*args` through a per-position unroll,
  ddb2d555. `msp_ss`'s `'>H8xBB4x'` needed nothing.)

**`serial`** (2026-10-06) was a stub that returned `""` from `read` and
took only `(port, baudrate)`. It now models the pyserial 3.5 API surface.
pyc cannot configure a serial port (that needs termios), so opening one
raises `SerialException`, and I/O on an unopened port raises
`PortNotOpenError`, exactly as pyserial does for a port it cannot open.
That depended on ifa/049: a model whose operations can only raise was
refused until then.

## Audit to repeat

Grep `pyc_lib/*.py` for functions whose body returns a constant, and for
`# stub` comments. Each hit is either implemented or made to raise.

## Verification

Each implemented function matches CPython byte-for-byte in a `tests/`
fixture on both backends. Each unimplemented one raises, with a test
showing that it does.
