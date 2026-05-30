# pyc tests

End-to-end test suite for the pyc Python-to-C compiler.

## Running

```bash
make test         # everything: unit + e2e
make test-e2e     # only e2e (= ./test_pyc)
make test-unit    # IFA's unit tests via ifa/ifa --test
make test-dparse  # parse-only validation of every tests/*.py
```

Direct invocation of the e2e runner:

```bash
./test_pyc                       # quiet mode, full suite
./test_pyc -v                    # verbose: show every PASS line
./test_pyc class                 # only tests matching *class*
./test_pyc --no-cpython          # skip CPython cross-verify
./test_pyc -k class_inheritance  # keep tests/build/ for inspection
./test_pyc -b                    # stop at first failure
./test_pyc -t 120                # per-step timeout 120s (default: 60)
./test_pyc -h                    # all options
```

Environment overrides: `PYC`, `PYTHON`, `VALGRIND`, `TIMEOUT`.

## Categories

- **Unit tests** — IFA library internals via the `UnitTest` framework
  (`ifa/common/unit.cc`). Registered in `ifa/common/vec_test.cc` and any
  other `*_test.cc`. Run via `ifa/ifa --test`.
  No pyc-side unit tests exist yet; new ones can be added by linking a
  C++ file that uses `UNIT_TEST_FUN(my_test)`.
- **E2E tests** — `tests/*.py` compiled by pyc and (optionally) executed
  and diffed against CPython. Run via `./test_pyc`.
- **DParser parse tests** — `make test-dparse`. Only verifies that each
  `tests/*.py` parses without error, no compile/execute.
- **Regression** — same files as E2E. Outputs are blessed via
  `./baseline` (see below).

## Test layout

Each test is a single Python file `tests/foo.py` plus zero or more
sidecar files driven by the runner:

| Sidecar | Effect |
|---|---|
| `foo.py.flags` | extra pyc CLI flags |
| `foo.py.check` | expected compile-time stdout |
| `foo.py.exec.check` | expected runtime stdout (if present, the test is also executed) |
| `foo.py.python.expect_fail` | CPython produces different output (known divergence) |
| `foo.py.expect_fail` | pyc is expected to fail compilation |
| `foo.py.check_fail` | pyc may fail; counted as pass either way |
| `foo.py.ignore` | skip this test |

All other files in `tests/` are ignored by the runner, so fixture files
like `tests/t34_import.py` (used as an import target by
`module_import.py` but flagged `.ignore` itself) work fine.

## Adding a test

```bash
./add_test my_local.py                 # uses basename as test name
./add_test my_local.py custom_name.py  # explicit name
```

`add_test`:
1. Copies the source into `tests/`.
2. Compiles it with pyc.
3. Captures the runtime output as `tests/<name>.py.exec.check`.
4. Runs `git add` on both.

You should then run `./test_pyc <pattern>` to verify the new test passes.

## Re-blessing baselines

When intentional output changes need to update `.check` files:

```bash
./test_pyc -k <pattern>     # keeps tests/build/ around
./baseline <pattern>        # copies build/*.out → tests/*.check
git diff tests/             # ALWAYS inspect
git add tests/...
```

Without a pattern `./baseline` operates on all current build outputs.

## Where outputs go

The runner stages every `tests/*.py` as a symlink into `tests/build/`
and runs pyc + the compiled binary + CPython all with `tests/build/` as
cwd. Outputs land in `tests/build/` and are wiped at end of run (use
`-k` to keep). The source tree (`tests/*.py` + sidecars) is never
written to.

`tests/build/` is `.gitignore`'d.

## Failure modes

| Outcome | Meaning |
|---|---|
| `PASS` | compiled + (if exec.check) ran + (if cpython) matched CPython |
| `XFAIL compile` | pyc failed, `.expect_fail` present → counted as expected |
| `XFAIL cpython` | CPython diverged from pyc, `.python.expect_fail` present |
| `SKIP` | `.ignore` sidecar present |
| `FAIL COMPILE` | pyc returned non-zero unexpectedly |
| `FAIL COMPILE-TIMEOUT` | pyc didn't finish within `TIMEOUT` seconds |
| `FAIL COMPILE-OUT` | pyc's stdout differs from `.check` |
| `FAIL EXEC` | compiled binary's stdout differs from `.exec.check` |
| `FAIL EXEC-TIMEOUT` | compiled binary didn't finish |
| `FAIL CPYTHON` | pyc's exec stdout differs from CPython's |

On any FAIL the runner exits 1.

## Debugging a failure

```bash
./test_pyc -k -v <pattern>          # keep build/ around, show all
ls tests/build/<test>.py.*          # inspect compile/exec/python outputs
diff tests/build/<test>.py.exec.out tests/<test>.py.exec.check
```

You can also re-run pyc by hand from `tests/build/`:

```bash
cd tests/build
../../pyc -D../.. -v -d <test>.py   # add -v/-d for verbose/debug
./<test>                            # run the compiled binary
```

For very stuck tests, add a `.ignore` sidecar to skip without removing
the source: `touch tests/<test>.py.ignore`.

## Notes

- Per-test timeout defaults to 60s and can be raised via `-t` or
  `TIMEOUT=`. A timeout produces `FAIL ... -TIMEOUT` and is counted as
  a failure (not a skip).
- `pyc_compat.py` is auto-copied into `tests/build/` so user programs
  that `from pyc_compat import __pyc_declare__` continue to import OK
  under CPython.
- The `empty` file is a 0-byte fixture used as the diff target for
  tests with no `.check` file. Don't delete it.
- See [../PYTHON_FRONTEND.md](../PYTHON_FRONTEND.md) for the
  Python-frontend internals and
  [../PIPELINE.md](../PIPELINE.md) for the end-to-end compile flow.
