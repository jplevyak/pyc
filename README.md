# pyc — Python Compiler

**pyc** is a whole-program, statically typed compiler for Python 3. It infers a type for every
value in the program with interprocedural flow analysis (IFA), then emits C (or LLVM IR) and
compiles it to a stand-alone native binary with no Python runtime dependency.

The goal is **CPython semantics** for programs that can be typed without boxing: a compiled
program prints what CPython prints, or pyc refuses it at compile time. Where pyc accepts a
Python idiom that strict static typing cannot express exactly, the accommodation is named, warns
when it changes behaviour, and is refused under `--strict` (see [Modes](#modes)).

## Features

- **Type inference with no annotations.** IFA is a simultaneous data- and control-flow analysis
  by abstract interpretation over a type lattice. It starts from one contour per function and
  per allocation site, and splits contours only when a type distinction demands it. Each
  resulting specialization of a function or class is compiled separately.
- **Two backends.** The C backend (the default) emits C compiled by the system C compiler at `-O2`.
  The LLVM backend (`-b`) emits LLVM IR directly. It also powers a JIT (`-j`) and an
  interactive REPL (`--repl`). CI runs the whole test suite on both backends.
- **Boehm GC** for memory management.
- **Its own parser.** Python 3 syntax is parsed by a DParser grammar. CPython isn't needed at
  compile time or at run time.

### Supported Python

- **Classes**: inheritance, `super()`, class attributes, operator overloading (`__add__`,
  `__getitem__`, `__eq__`, …), `__str__`/`__repr__`, `__iter__`/`__next__`, `__enter__`/`__exit__`,
  read-only `@property`.
- **Functions**: closures, `nonlocal`/`global`, lambdas, default and mutable default
  arguments, `*args`, nested functions, local classes.
- **Exceptions**: `try`/`except`/`else`/`finally`, `raise`, user exception hierarchies, and
  CPython's built-in exceptions raised by the runtime (`ValueError` from `int("x")`,
  `IndexError`, `KeyError`, `StopIteration`, …).
- **Generators**: `yield`, `yield from`, `send()`, and return values.
- **`async`/`await`**, run on pyc's own event loop (there is no `asyncio` module).
- **Control flow**: `if`/`elif`/`else`, `for`/`while` with `break`/`continue`/`else`, `with`,
  `match`/`case` (all PEP 634 pattern kinds), comprehensions and generator expressions,
  starred assignment (`a, *b = xs`).
- **Built-in types**: `int` (64-bit), `float`, `bool`, `str`, `bytes`, `bytearray`, `list`,
  `tuple`, `dict`, `set`, `range`, `slice`, `None`, with `%`-formatting and f-strings.
- **Modules**: `import`, `from … import`, packages and relative imports, all resolved at compile
  time. Partial implementations of common standard-library modules live in
  [`pyc_lib/`](pyc_lib), including `math`, `random`, `time`, `sys`, `os`, `collections`, `heapq`,
  `copy`, `struct`, `string`, `socket` and parts of `re` and `itertools`. File I/O uses `open()`.

### Language extensions

pyc exposes a small set of compiler directives. Its builtin library (`__pyc__/`) is written with
them:

| Name | Purpose |
|---|---|
| `__pyc_c_call__(ret, fn, ...)` | Inline a raw C function call |
| `__pyc_primitive__(sym, ...)` | Invoke a compiler primitive |
| `__pyc_char__` | The `uint8` / C `char` type |
| `__pyc_operator__` | Access C-level operators directly |
| `__pyc_insert_c_header__(h)` | Include a C header in the generated code |
| `@vector("s")` | Annotate a class as a fixed-size value-type vector |

A field that holds different types in different objects needs no declaration: a class whose
`__init__` stores an `int` in one instance and a `str` in another gets one layout per type.

## Requirements

- **clang/clang++** (C++23) and **llvm-ar**
- **LLVM** development libraries (`llvm-config` must be on `PATH`); the LLVM backend is always
  built
- **Boehm GC** (`libgc`, `libgccpp`)
- **PCRE** (`libpcre`)
- **[dparser](https://github.com/jplevyak/dparser)**, built with GC support and installed

On Ubuntu / Debian:

```sh
apt-get install clang llvm-dev libgc-dev libpcre3-dev
```

On Fedora / RHEL:

```sh
dnf install clang llvm-devel gc-devel pcre-devel
```

CI uses clang/LLVM **20** on Ubuntu 24.04. The unversioned packages there are LLVM 18, whose
coroutine ABI breaks the generator and async tests.

## Building

`ifa` (the IFA library) is included as a git subtree under `ifa/` and is built automatically.
Build and install `dparser` separately first:

```sh
# 1. Build and install dparser (with GC support)
git clone https://github.com/jplevyak/dparser.git
cd dparser
sudo make install D_USE_GC=1
cd ..

# 2. Clone and build pyc
git clone https://github.com/jplevyak/pyc.git
cd pyc
make
```

`make` builds `ifa/libifa_gc.a`, the runtime and the `pyc` compiler binary.

### Build options

| Variable | Effect |
|---|---|
| `DEBUG=1` | Adds `-g -DDEBUG` (default) |
| `OPTIMIZE=1` | Builds pyc itself with `-O3 -march=native` (default; `OPTIMIZE=` turns it off) |
| `PROFILE=1` | Enable profiling with `-pg` |

For an unoptimized build to step through in gdb, use `make clean && make OPTIMIZE=`.

The Makefiles track neither build flags nor all header dependencies. Run `make clean` after
changing a flag or a header; otherwise stale objects can produce failures that look like real bugs.

## Usage

```sh
pyc -D <pyc-dir> [options] <file.py>
```

pyc reads `<file.py>` and everything it imports, analyzes the whole program, and writes a native
executable named after the input with its extension stripped (`hello.py` → `hello`), unless `-o`
names one. The intermediate C is kept as `<file.py>.c`.

`-D` names the directory holding pyc's builtin library (`__pyc__/`, `pyc_lib/` and the runtime).
It defaults to `$PYC_SYSTEM_DIRECTORY` when that is set, else the current directory, and when the
current directory has no `__pyc__/`, the directory of the `pyc` executable itself (through any
symlink). So a `pyc` symlinked onto your `PATH` from the checkout works from anywhere.

### Running a script directly

```python
#!/usr/bin/env -S pyc -x
import sys
print("hello", sys.argv[1:])
```

With that first line and the executable bit set, `./hello.py a b` compiles the script once and
runs it, passing it `a b`. `-x` keeps the executable in `$XDG_CACHE_HOME/pyc` (default
`~/.cache/pyc`), in a directory named after the script and a hash of its content, its path, the
`pyc` binary and the options. The directory also holds a manifest of every source file the compile
read (the script, its imports, `pyc_lib` modules and the builtin library) with their content
hashes. A later run whose sources are all unchanged starts the cached executable directly, in a
few milliseconds; if any changed, it recompiles. Nothing is written beside the script, its exit
status is the program's, and `sys.argv[0]` is the script's path, as under CPython. Other options
go before `-x`'s script, as in `#!/usr/bin/env -S pyc -b -x`.

### Options

| Flag | Description |
|---|---|
| `-D <dir>`, `--system-directory <dir>` | pyc's library directory (default `$PYC_SYSTEM_DIRECTORY`, else `.`, else the `pyc` executable's directory) |
| `-o <path>`, `--output <path>` | Output binary path (default: input filename, extension stripped) |
| `-x`, `--exec` | Compile (cached in `~/.cache/pyc`) and run; arguments after the script go to the program. For `#!` lines; see above |
| `--strict` | CPython semantics or a compile error: every type violation is fatal, no accommodations |
| `--permissive` | The default: named accommodations are allowed, each with a warning |
| `--safe` | Permissive, and also zero-initialize locals that may be read before assignment |
| `--refuse-imprecise` | Refuse a program whose inference left a container element or parameter untyped |
| `-b`, `--emit-llvm` | Use the LLVM backend |
| `-j`, `--jit` | Compile with the LLVM backend and run in-process (implies `-b`) |
| `--repl` | Interactive REPL (uses the JIT) |
| `-O`, `--optimize` | Compile the generated C at `-O2` (the default) |
| `--no-optimize` | Compile the generated C at `-O0`, for debugging |
| `-g`, `--debug-info` | Emit debug information |
| `--html` | Emit an HTML visualization of the type-annotated program |
| `-v`, `--verbose` / `-d`, `--debug` | Increase verbosity / debug output (repeat for more) |
| `--version`, `--license`, `-h`, `--help` | Version, license, help (`--help` also lists development options) |

### Modes

Static typing without boxing cannot express everything CPython does. pyc's rule is that it never
deviates silently:

- **`--strict`**: the program has CPython's semantics, or it does not compile. Gaps in this
  guarantee are bugs, tracked as silent wrong answers in [issues/](issues/README.md).
- **`--permissive`** (the default) additionally allows a few named accommodations. Examples: a
  numeric slot that holds both `int` and `float` is widened to `float`, and a generator's
  `next()` resume delivers `0` instead of `None` when the generator is also sent numbers. Each
  one is behind a named flag, warns at the place it applies, and is off under `--strict`.

A value that genuinely holds two unrelated types, such as an `int` in one branch and a `str` in
the other, has no representation. pyc refuses it in every mode and names the variable and the
union.

### Module search path

Imports resolve against the **current directory**, then `<pyc-dir>/pyc_lib/`, then each
directory in `PYTHONPATH`. The source file's own directory is not searched, so compile from the
program's directory, or add it to `PYTHONPATH`:

```sh
PYTHONPATH=/my/libs pyc -D ~/pyc myprogram.py
```

## Examples

### Hello world

```python
# hello.py
print("Hello, world!")
```

```sh
pyc -D ~/pyc hello.py && ./hello
```

### Fibonacci

```python
def fib(x):
    if x == 0 or x == 1:
        return 1
    else:
        return fib(x-2) + fib(x-1)

print(fib(33))
```

### Classes and inheritance

```python
class A(object):
    n = 2
    def __init__(self, a):
        print(a + 10)
    def method(self):
        return self.n

class B(A):
    def __init__(self, a):
        super(B, self).__init__(a)
        print(self.n)

y = B(3)
```

### List comprehensions

```python
x = [1, 2, 3]
y = [5, 6, 7]
z = [i + j + 1 for i in x for j in y]
print(z)
```

### Closures and default arguments

```python
def f(a, L=[]):
    L.append(a)
    return L

print(f(1))   # [1]
print(f(2))   # [1, 2]
print(f(3))   # [1, 2, 3]
```

### Polymorphic fields

A field may hold a different type in each object. No declaration is needed: each type gets its
own layout of `C`.

```python
class C:
    def __init__(self, val):
        self.value = val

print(C(1).value)
print(C("hello").value)
print(C(3.14).value)
```

## Testing

```sh
make test          # everything CI runs, in CI's order, on both backends (~4 minutes)
```

`make test` chains four groups, each of which also runs on its own:

```sh
make test-core       # ifa unit tests, IR phase tests, and the end-to-end suite
                     # (./test_pyc.py) on the C backend, then on LLVM
make test-ifa-llvm   # ifa V-language LLVM smoke (= make -C ifa test_llvm)
make test-dparse     # DParser parse-only validation of every test file
make test-links      # every relative Markdown link resolves (needs no build)
```

The end-to-end suite compiles each `tests/*.py`, runs it, and compares its output with the
expected output and with CPython's. When green it reports 488 passed and 14 known issues on each
backend. Tests carrying a `.known_issue` sidecar are filed bugs with CPython's answer as the
expected output; they report `KNOWN` and flip to `PASS` once fixed. `make test` stops at the first
failing group, so check its exit status rather than reading the tail of its output.

Beware a name trap: this directory's `make test_llvm` is the pyc end-to-end suite on the LLVM
backend, while `make -C ifa test_llvm` is the V-language smoke (`make test-ifa-llvm`).

### The shedskin corpus

[`shedskin_examples/`](shedskin_examples) holds shedskin's 77 example programs, pyc's larger
benchmark. `./corpus_sweep.sh -m check` compiles and runs each one and compares its output with
CPython's. Results are cached under [`sweeps/`](sweeps/INDEX.md), keyed on the source tree. As of
2026-10-16:

- 76 of 77 programs compile. `othello3` exceeds the 400 s compile cap.
- All 76 binaries run to completion.
- 58 print output byte-identical to CPython's, and 1 (`othello2`) differs.
- The other 17 can't be compared because CPython itself exceeds the 120 s run cap.

## Architecture

```
Python source (.py)
  → DParser (python.g)          — parse to PyDAST
  → build_syms                  — symbol tables, scopes, classes
  → build_if1                   — lower to IF1, ifa's intermediate form
  → IFA flow analysis           — type inference with demand-driven contour splitting
  → clone                       — one concrete function/type per contour
  → optimize                    — dead code, inlining, constant folding
  → C codegen  → cc → binary
    or LLVM codegen → binary / JIT
```

The IFA engine (`ifa/`) is a general-purpose interprocedural analysis library that pyc drives
through the `IFACallbacks` interface. Every value is typed by the set of concrete types that can
reach it.

Where to read next:

- [PIPELINE.md](PIPELINE.md): phase-by-phase map from `pyc file.py` to the binary
- [PYTHON_FRONTEND.md](PYTHON_FRONTEND.md): the Python lowering
- [RUNTIME.md](RUNTIME.md): the C runtime and the `__pyc__/` builtin library
- [AGENTS.md](AGENTS.md): the design rules (demand-driven splitting, CPython semantics, no
  boxing) and the change-acceptance process
- [ifa/INDEX.md](ifa/INDEX.md): the IFA library's own documentation

## Limitations

pyc compiles a large subset of Python 3, not all of it. Bugs and gaps are tracked in
[issues/](issues/README.md) (frontend, runtime, harness) and [ifa/issues/](ifa/issues)
(analysis and codegen). Known unsupported features include:

- **Dynamic features**: `eval`, `exec`, and runtime reflection are not supported.
- **Values that need boxing.** A variable, field or container element that genuinely holds two
  unrelated types over time (`{int, str}`, `{None, int}`) has no representation and is refused.
- **Parameters**: `**kwargs` and keyword-only parameters (`def f(a, *, b)`) are not supported
  ([issues/182](issues/182-kwargs-and-keyword-only-parameters.md)).
- **Properties**: only read-only `@property` getters. Setters and deleters are refused
  ([issues/007](issues/007-decorators-not-applied.md)).
- **`complex`** is not supported.
- **`@classmethod`**: reading an attribute through `cls` (`cls.base`, `cls.other()`) does not
  compile; `cls(...)` does ([issues/184](issues/184-classmethod-attribute-through-cls.md)).
- **Generators**: `StopIteration.value` read in
  an `except` clause is refused when the program raises `StopIteration` with more than one value
  type ([issues/172](issues/172-generator-send-channel-and-stopiteration-value.md)).
- **The standard library** is limited to the partial modules in `pyc_lib/`.
- **Class attribute mutation through a subclass**: `A.n = 4` is not seen by `B.n` when `B`
  inherits `n` from `A`, because each class holds its own copy
  ([issues/173](issues/173-silent-deviations-found-by-the-strict-suite-check.md) #2).
- **Cross-class method assignment**: `a.x = b.x`, where `a` and `b` are instances of unrelated
  classes, is not supported; it currently compiles and crashes at run time
  ([issues/180](issues/180-method-assigned-across-classes-crashes.md)).
- **A known silent wrong answer** under repair: an out-of-range sequence index does not raise
  `IndexError` ([issues/179](issues/179-sequence-index-out-of-range-does-not-raise.md)).

## License

BSD 3-Clause — see [LICENSE](LICENSE).

Copyright (c) John Bradley Plevyak.
