# pyc — Python Compiler

**pyc** is a whole-program, statically-typed compiler for a substantial subset of Python 3. It uses
interprocedural flow analysis (IFA) to infer types throughout the entire program and then emits C
code that is compiled by the host C compiler.  The result is a stand-alone native binary with no
Python runtime dependency.

## Features

- **Type inference** — Hindley-Milner-style interprocedural flow analysis infers types without any
  annotations.  Each monomorphic specialization of a polymorphic function gets its own compiled
  version.
- **Native code generation** — emits readable C, compiled with the system C compiler (`cc`).
  An experimental LLVM backend is also included.
- **Boehm GC** — uses the Boehm garbage collector so Python-style memory semantics are preserved.
- **Python 3 syntax** — parsed by a custom DParser-based parser (no CPython runtime required at
  compile time or at run time).
- **Object-oriented Python** — classes, single inheritance, `super()`, `__init__`, operator
  overloading (`__add__`, `__getitem__`, etc.), descriptors.
- **First-class functions** — closures, lambdas, default arguments, mutable default arguments.
- **Built-in types** — `int`, `float`, `complex`, `str`, `bool`, `list`, `tuple`, `dict`
  (structural), `bytearray`, `range`, plus the usual numeric operators.
- **Control flow** — `if`/`elif`/`else`, `for`/`while`/`break`/`continue`/`else`, list
  comprehensions, generator expressions.
- **Scoping** — full Python scoping rules including `global`, `nonlocal`, implicit capture, class
  scopes, nested functions.
- **Imports** — module imports resolved at compile time; imported module code is inlined.
- **`pyc_compat`** — a small compatibility shim (`from pyc_compat import __pyc_declare__`) that lets
  you declare polymorphic record fields while keeping the file runnable under standard Python.

### Language extensions

pyc exposes a small set of compiler directives accessible from Python:

| Name | Purpose |
|---|---|
| `__pyc_declare__` | Declare a class field as polymorphic (union type) |
| `__pyc_c_call__(ret, fn, ...)` | Inline a raw C function call |
| `__pyc_primitive__(sym, ...)` | Invoke a compiler primitive |
| `__pyc_char__` | The `uint8` / C `char` type |
| `__pyc_operator__` | Access C-level operators directly |
| `@vector("s")` | Annotate a class as a fixed-size value-type vector |

## Requirements

- **clang++** (C++23) and **llvm-ar**
- **Boehm GC** (`libgc`, `libgccpp`)
- **PCRE** (`libpcre`)
- **[dparser](https://github.com/jplevyak/dparser)** — must be built with GC support and installed

On Ubuntu / Debian:

```sh
apt-get install clang llvm-dev libgc-dev libpcre3-dev
```

On Fedora / RHEL:

```sh
dnf install clang llvm-devel gc-devel pcre-devel
```

## Building

`ifa` (the IFA library) is included as a git subtree under `ifa/` and is built automatically.
`dparser` must be built and installed separately (see below).

```sh
# 1. Build and install dparser (with GC support)
git clone https://github.com/jplevyak/dparser.git
cd dparser
sudo make install D_USE_GC=1
cd ..

# 2. Clone and build pyc (ifa is already included as a subtree)
git clone https://github.com/jplevyak/pyc.git
cd pyc
make
```

The `make` step builds `ifa/libifa_gc.a` and the `pyc` compiler binary.

### Build options

| Variable | Effect |
|---|---|
| `DEBUG=1` | Debug build with `-g -DDEBUG` (default) |
| `OPTIMIZE=1` | Optimized build with `-O3 -march=native` |
| `PROFILE=1` | Enable profiling with `-pg` |
| `USE_LLVM=1` | Enable experimental LLVM backend |

Example: `make OPTIMIZE=1`

## Usage

```sh
pyc [options] <file.py>
```

The compiler reads `<file.py>`, type-checks the whole program, and produces a native executable
named `<file.py>.out` (via an intermediate `<file.py>.c`).

### Key options

| Flag | Description |
|---|---|
| `-D <dir>` | System directory containing `__pyc__.py` (default: same directory as `pyc`) |
| `-O` | Enable optimizations |
| `-g` | Emit debug information |
| `-r` | Insert runtime type checks |
| `--html` | Emit an HTML visualization of the type-annotated AST |
| `-v` | Increase verbosity (repeat for more) |
| `-d` | Increase debug output (repeat for more) |
| `--dparse_only` | Validate the parser only; do not compile |
| `--dparse_ast` | Print the parsed AST and exit |
| `--version` | Show version |
| `--license` | Show license |
| `-h` | Show help |

### Module search path

pyc searches for imported modules starting from the directory of the source file being compiled.
Set `PYTHONPATH` to add additional directories:

```sh
PYTHONPATH=/my/libs pyc myprogram.py
```

## Running the tests

```sh
make test          # run all functional tests (expects 2 known failures: t18, t30)
make test_dparse   # run DParser parse-only validation on all test files
```

## Examples

### Hello world

```python
# hello.py
print("Hello, world!")
```

```sh
pyc -D. hello.py && ./hello.py.out
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

### Polymorphic fields with `__pyc_declare__`

```python
from pyc_compat import __pyc_declare__

class C:
    value = __pyc_declare__   # accepts int, str, float, …
    def __init__(self, val):
        self.value = val

print(C(1).value)
print(C("hello").value)
print(C(3.14).value)
```

## Architecture

```
Python source (.py)
  → DParser (custom grammar, no CPython)
  → PyDAST (DParser AST)
  → build_syms  — symbol table, scope resolution
  → build_if1   — lower to IF1 intermediate form
  → IFA          — interprocedural type inference / flow analysis
  → C code generation
  → cc / clang   — native binary
```

The IFA engine (`ifa/`) is a general-purpose interprocedural analysis library used here as the
type inference and code-generation backend.  It operates on an IF1-style functional intermediate
representation where every value is typed by the set of concrete types that can flow to it.

## Limitations

pyc targets a substantial but not complete subset of Python 3.  Known unsupported features
include exceptions (`try`/`except`/`raise`), generators (`yield`), dictionaries as first-class
values, starred assignment, `exec`, `eval`, and most of the standard library.  Programs that stay
within the supported subset and rely on static type structure compile and run correctly.

### Runtime class attribute mutation through inheritance (t18)

Assigning to a class attribute at runtime (e.g., `A.n = 4`) does not propagate to subclasses
that inherited the attribute (e.g., `B.n` still returns the original value).  pyc compiles class
attribute access as direct C struct field access, with inheritance resolved entirely at compile
time.  Propagating runtime attribute mutations across an inheritance hierarchy would require a
dynamic dispatch mechanism incompatible with the static struct layout used throughout the
generated code.

### Cross-type method assignment (t30)

Assigning a method from one class instance to another (e.g., `a.x = b.x` where `a` and `b` are
instances of different classes) produces a compile error.  pyc's type inference creates
distinct, incompatible closure struct types for methods of different classes.  Unifying these
types at runtime would require a dynamic typing layer that is fundamentally at odds with the
whole-program static type analysis that makes pyc's output efficient.

## License

BSD 3-Clause — see [LICENSE](LICENSE).

Copyright (c) John Bradley Plevyak.
