# pyc Roadmap — Python 3 Feature Completion

## Overview

**pyc** compiles a static subset of Python 3 to native C using whole-program type inference.
Every feature added must be compatible with that model: types are resolved at compile time,
polymorphism is handled by monomorphic cloning, and the generated C is readable.

This roadmap lists missing Python 3 features in priority order, broken into phases sized for
one LLM coding session (roughly 1–2 hours each). Each phase identifies the files to touch,
the test cases that should pass when it is done, and any known constraints.

### Fundamental limits (by design)

These features conflict with static whole-program typing and will **not** be implemented:

- `exec` / `eval` / dynamic `import`
- `setattr(obj, runtime_string, value)` / `getattr` with non-literal names
- Full metaclass protocol
- `inspect` / `pickle` / `marshal`
- CPython C-extension API

---

## Current State (quick reference)

| Feature | Status |
|---|---|
| int / float / complex / bool | ✓ |
| str (operators only) | partial — methods missing |
| list (append, index, count, iter, slice) | partial |
| tuple / range / bytearray | ✓ |
| dict | parsed only — no code gen |
| set / frozenset | absent |
| if / while / for / break / continue | ✓ |
| for-else / while-else | ✓ (grammar+runtime) |
| List comprehensions | ✓ |
| Dict / set comprehensions | parsed only |
| Generator expressions | parsed only |
| Classes, single inheritance, super() | ✓ |
| Closures / lambdas / defaults | ✓ |
| global / nonlocal scoping | ✓ |
| Module imports | ✓ |
| try / except / raise | parsed — not compiled |
| with / as | parsed — not compiled |
| yield / generators | parsed — not compiled |
| assert | parsed — not compiled |
| *args / **kwargs (definitions) | parsed — not compiled |
| Keyword arguments at call sites | partial |
| f-strings | not in grammar |
| Type annotations | not in grammar |
| Walrus := | not in grammar |
| @classmethod / @staticmethod | not implemented |
| @property | not implemented |
| Multiple inheritance | not implemented |

---

## Milestone 1 — Dict, Set, and Comprehensions

*No new grammar needed; the parser already produces these nodes.*

### Phase 1.1 — Dict literals and dict class (~1.5 h)

**Goal:** `{k: v}` literals compile; `dict` is a usable first-class type.

**Files:** `__pyc__.py`, `python_ifa_build_if1.cc` (`PY_dict` case),
`python_ifa_build_syms.cc` (`PY_dict` case), `pyc_c_runtime.h` (hash-map primitives if needed;
alternatively implement dict as a list of `(key, value)` pairs using the existing GC allocator).

**Tests to pass:**
```python
d = {'a': 1, 'b': 2}
print(d['a'])          # 1
d['c'] = 3
print(d['c'])          # 3
print(len(d))          # 3

def f(x):
    return {'x': x, 'y': x * 2}
r = f(4)
print(r['x'], r['y'])  # 4 8

# dict methods
d2 = {'x': 10}
print(d2.get('x', 0))   # 10
print(d2.get('z', 99))  # 99
d2.update({'y': 20})
print(d2['y'])           # 20
for k in d2:
    print(k)             # x  y (order may vary)
```

**Implementation note:** A structurally-typed dict maps compile-time-known key-type →
value-type. The simplest correct implementation is a linked list of `(key, value)` cells
using `__eq__` for lookup. This is O(n) but correct and generates simple C. A hash-map
can be added later as an optimization.

---

### Phase 1.2 — Set and frozenset (~1.5 h)

**Goal:** `{x, y}` set literals; basic set operations.

**Files:** `python_ast.h` (already has `PY_set`), `__pyc__.py` (add `class set`),
`python_ifa_build_if1.cc` (`PY_set` case).

**Tests to pass:**
```python
s = {1, 2, 3}
print(len(s))         # 3
print(2 in s)         # True
print(5 in s)         # False
s.add(4)
s.discard(1)
print(len(s))         # 3

a = {1, 2, 3}
b = {2, 3, 4}
print(a & b)          # {2, 3}
print(a | b)          # {1, 2, 3, 4}
print(a - b)          # {1}

fs = frozenset([1, 2, 3])
print(2 in fs)        # True
```

---

### Phase 1.3 — Dict, set, and generator comprehensions (~1.5 h)

**Goal:** `{k: v for ...}`, `{x for ...}`, `(x for ...)`.

**Files:** `python_ifa_build_if1.cc` and `python_ifa_build_syms.cc` — `PY_dict` with
children pattern `[key_expr, val_expr, PY_comp_for...]`, `PY_set` with one expr +
comp_for, `PY_genexpr`.

**Tests to pass:**
```python
pairs = [('a', 1), ('b', 2)]
d = {k: v for k, v in pairs}
print(d['a'])    # 1

nums = [1, 2, 3, 4, 5]
evens = {x for x in nums if x % 2 == 0}
print(len(evens))  # 2
print(2 in evens)  # True

gen = (x * x for x in range(5))
print(next(gen))   # 0   (requires Phase 4)
```

---

## Milestone 2 — Exception Handling

*The grammar already parses all try/except/raise/assert syntax.*

### Phase 2.1 — Runtime exception infrastructure (~1.5 h)

**Goal:** Add C runtime support for raising and catching typed exceptions.

**Files:** `pyc_c_runtime.h` (add `_CG_PUSH_TRY`, `_CG_POP_TRY`, `_CG_RAISE` macros
using `setjmp`/`longjmp`; add `_CG_exception_state` thread-local struct),
`__pyc__.py` (add `class BaseException`, `class Exception`, `class ValueError`,
`class TypeError`, `class IndexError`, `class KeyError`, `class AttributeError`,
`class StopIteration`, `class RuntimeError`, `class ZeroDivisionError`,
`class NameError`, `class NotImplementedError`, `class OSError`).

**Implementation note:** Use a global (or thread-local) exception stack of `setjmp`
buffers. `_CG_RAISE(exc_ptr)` saves the exception pointer in a global, then `longjmp`s
to the nearest handler. Each exception class is a C struct with a `char *message` field
and a type tag (integer matching a static hierarchy).

No test cases yet — this is infrastructure. Phase 2.2 adds the compiler support.

---

### Phase 2.2 — try / except / raise (~2 h)

**Goal:** Full compile support for `try/except/raise`.

**Files:** `python_ifa_build_syms.cc` (scoping for `PY_try_stmt`, `PY_except_handler`,
`PY_raise_stmt`), `python_ifa_build_if1.cc` (code generation using the Phase 2.1 macros).

**Tests to pass:**
```python
# basic raise/except
try:
    raise ValueError("bad value")
except ValueError as e:
    print("caught ValueError")   # caught ValueError

# exception hierarchy
def risky(x):
    if x < 0:
        raise ValueError("negative")
    return x * 2

try:
    risky(-1)
except Exception as e:
    print("caught")              # caught

# bare except
try:
    raise RuntimeError("oops")
except:
    print("bare except")        # bare except

# re-raise
def wrap():
    try:
        raise TypeError("t")
    except TypeError:
        raise

try:
    wrap()
except TypeError:
    print("re-raised")          # re-raised

# assert
assert 1 == 1
try:
    assert 1 == 2, "one is not two"
except AssertionError as e:
    print("assert failed")      # assert failed
```

---

### Phase 2.3 — try / except / else / finally (~1 h)

**Goal:** `else` and `finally` clauses on try blocks.

**Files:** `python_ifa_build_if1.cc` (extend `PY_try_stmt` code gen).

**Tests to pass:**
```python
# else clause runs if no exception
try:
    x = 1
except ValueError:
    print("err")
else:
    print("ok")        # ok

# finally always runs
def f():
    try:
        return 1
    finally:
        print("finally")    # finally

print(f())              # 1

# finally with exception
try:
    try:
        raise ValueError("v")
    finally:
        print("cleanup")    # cleanup
except ValueError:
    print("caught")         # caught
```

---

### Phase 2.4 — Context managers (with / as) (~1.5 h)

**Goal:** `with expr as name:` using `__enter__` / `__exit__` protocol.

**Files:** `python_ifa_build_syms.cc` (`PY_with_stmt`), `python_ifa_build_if1.cc`
(generate `__enter__` call, try/finally wrapper with `__exit__` call).

**Tests to pass:**
```python
class Resource:
    def __enter__(self):
        print("open")
        return self
    def __exit__(self, exc_type, exc_val, exc_tb):
        print("close")
        return False

with Resource() as r:
    print("using")
# open
# using
# close

# exception in body calls __exit__
try:
    with Resource() as r:
        raise ValueError("x")
except ValueError:
    print("caught")
# open
# close
# caught
```

---

## Milestone 3 — Generators

### Phase 3.1 — Basic generators (~2 h)

**Goal:** Functions containing `yield` compile to generator objects.

**Files:** `python_ifa_build_syms.cc` (detect yield in function body, mark as generator),
`python_ifa_build_if1.cc` (`PY_yield_stmt`, `PY_yield_expr` cases; generate state-machine
struct with `__next__`, `__pyc_more__`), `__pyc__.py` (add `next(g)` built-in,
`StopIteration` used as sentinel).

**Implementation note:** Transform a generator function into a struct holding the
"program counter" (an integer tag), all live locals, and a `__next__` method that
switches on the PC to resume at the right point. This is the same approach used
internally for `range` and `__list_iter__`.

**Tests to pass:**
```python
def count_up(n):
    i = 0
    while i < n:
        yield i
        i += 1

g = count_up(3)
print(next(g))   # 0
print(next(g))   # 1
print(next(g))   # 2

for x in count_up(4):
    print(x)     # 0 1 2 3

def fib():
    a, b = 0, 1
    while True:
        yield a
        a, b = b, a + b

g = fib()
for _ in range(7):
    print(next(g))   # 0 1 1 2 3 5 8
```

---

### Phase 3.2 — yield from and generator expressions (~1.5 h)

**Goal:** `yield from iterable`; `(expr for ...)` as lazy generators.

**Files:** `python_ifa_build_if1.cc` (extend yield code gen; `PY_genexpr` case).

**Tests to pass:**
```python
def chain(a, b):
    yield from a
    yield from b

for x in chain([1, 2], [3, 4]):
    print(x)    # 1 2 3 4

squares = (x * x for x in range(5))
print(list(squares))   # [0, 1, 4, 9, 16]  (requires list() built-in)
```

---

## Milestone 4 — String Methods

*All implemented in `__pyc__.py` as methods on `class str`; no grammar changes needed.*

### Phase 4.1 — Core string methods (~2 h)

**Goal:** The most commonly used string operations.

**Files:** `__pyc__.py` (add methods to `class str`), `pyc_c_runtime.h` (add C helpers
for split, join, replace using existing `PCRE` linkage where useful).

**Methods to implement:**
`split(sep=None, maxsplit=-1)`, `join(iterable)`, `strip()` / `lstrip()` / `rstrip()`,
`find(sub)` / `rfind(sub)`, `index(sub)` / `rindex(sub)`,
`startswith(prefix)` / `endswith(suffix)`,
`replace(old, new, count=-1)`,
`upper()` / `lower()` / `title()` / `capitalize()`,
`count(sub)`, `zfill(width)`, `center(w)` / `ljust(w)` / `rjust(w)`,
`isdigit()` / `isalpha()` / `isalnum()` / `isspace()` / `islower()` / `isupper()`.

**Tests to pass:**
```python
s = "hello world"
print(s.upper())              # HELLO WORLD
print(s.split())              # ['hello', 'world']
print(s.split('o'))           # ['hell', ' w', 'rld']
print(' '.join(['a', 'b']))   # a b
print(s.replace('world', 'pyc'))  # hello pyc
print(s.find('world'))        # 6
print(s.startswith('hello'))  # True
print(s.strip('  hi  '))      # hi   (strip variant)
print('  hi  '.strip())       # hi
print('42'.zfill(5))          # 00042
print('abc'.isalpha())        # True
print('123'.isdigit())        # True
```

---

### Phase 4.2 — str.format() and f-strings (~2 h)

**Goal:** `"text {}".format(val)` and `f"text {expr}"` string interpolation.

**Part A — str.format()** (no grammar change): add `format(*args)` to `class str`
in `__pyc__.py`. The runtime helper formats `{}`, `{0}`, `{name}` placeholders.

**Part B — f-strings** (grammar change required):
1. Add f-string token to `python.g` (`f"..."`, `f'...'`, triple-quoted variants)
2. Add `PY_fstring` and `PY_fstring_part` to `python_ast.h`
3. Regenerate `python.g.d_parser.cc`
4. Add `PY_fstring` case to `build_syms_pyda` and `build_if1_pyda` (lower to
   string concatenation via `str()` calls)

**Tests to pass:**
```python
name = "world"
print(f"hello {name}")          # hello world
x = 42
print(f"value is {x}")          # value is 42
print(f"{x * 2 + 1}")           # 85
print(f"{'nested'!r}")          # 'nested'
print("{} + {} = {}".format(1, 2, 3))   # 1 + 2 = 3
print("{x} {y}".format(x=1, y=2))       # 1 2
```

---

## Milestone 5 — List and Built-in Enhancements

### Phase 5.1 — Additional list methods (~1.5 h)

**Goal:** Complete the list API.

**Files:** `__pyc__.py` (add methods to `class list`).

**Methods to implement:**
`sort(key=None, reverse=False)`, `pop(i=-1)`, `insert(i, x)`,
`remove(x)`, `reverse()`, `copy()`, `clear()`, `extend(iterable)`.

**Tests to pass:**
```python
a = [3, 1, 4, 1, 5]
a.sort()
print(a)              # [1, 1, 3, 4, 5]
a.sort(reverse=True)
print(a)              # [5, 4, 3, 1, 1]

b = [1, 2, 3]
b.insert(1, 99)
print(b)              # [1, 99, 2, 3]
b.remove(99)
print(b)              # [1, 2, 3]
print(b.pop())        # 3
print(b)              # [1, 2]
b.extend([4, 5])
print(b)              # [1, 2, 4, 5]
b.reverse()
print(b)              # [5, 4, 2, 1]
c = b.copy()
c.clear()
print(len(c), len(b)) # 0 4
```

---

### Phase 5.2 — Core iteration built-ins (~1.5 h)

**Goal:** `enumerate`, `zip`, `map`, `filter`, `reversed`, `sorted`.

**Files:** `__pyc__.py` (implement as iterator classes), `pyc_symbols.h` (add symbols).

**Tests to pass:**
```python
for i, v in enumerate(['a', 'b', 'c']):
    print(i, v)     # 0 a / 1 b / 2 c

for a, b in zip([1, 2, 3], ['x', 'y', 'z']):
    print(a, b)     # 1 x / 2 y / 3 z

print(list(map(lambda x: x * 2, [1, 2, 3])))     # [2, 4, 6]
print(list(filter(lambda x: x > 2, [1, 2, 3, 4])))  # [3, 4]

for x in reversed([1, 2, 3]):
    print(x)        # 3 2 1

print(sorted([3, 1, 4, 1, 5]))           # [1, 1, 3, 4, 5]
print(sorted([3, 1, 4], reverse=True))   # [4, 3, 1]
print(sorted(['ba', 'a', 'ab'], key=lambda s: len(s)))  # ['a', 'ba', 'ab']
```

---

### Phase 5.3 — Aggregation and type-conversion built-ins (~1 h)

**Goal:** `sum`, `min`, `max` with key; `list()`, `tuple()`, `set()` from iterables.

**Files:** `__pyc__.py`.

**Tests to pass:**
```python
print(sum([1, 2, 3, 4]))        # 10
print(sum([1, 2, 3], 10))       # 16
print(min([3, 1, 4]))           # 1
print(max([3, 1, 4]))           # 4
print(min(3, 1, 4))             # 1
print(min([3, 1, 4], key=lambda x: -x))  # 4

print(list(range(5)))           # [0, 1, 2, 3, 4]
print(tuple([1, 2, 3]))         # (1, 2, 3)  -- repr TBD
print(set([1, 2, 2, 3]))        # {1, 2, 3}  (requires Phase 1.2)
```

---

## Milestone 6 — Variable Argument Functions

### Phase 6.1 — *args in function definitions (~2 h)

**Goal:** Functions collect extra positional arguments into a tuple.

**Files:** `python_ifa_build_syms.cc` (`PY_star_arg` node handling in `get_syms_args_pyda`
currently ignores variadic semantics), `python_ifa_build_if1.cc` (generate code to
build a tuple from surplus positional arguments at the call site).

**Tests to pass:**
```python
def f(*args):
    print(len(args))
    for a in args:
        print(a)

f(1, 2, 3)     # 3 / 1 / 2 / 3
f()            # 0

def g(x, *rest):
    print(x)
    print(rest)

g(1, 2, 3)     # 1 / (2, 3)
g(10)          # 10 / ()
```

---

### Phase 6.2 — **kwargs in function definitions (~1.5 h)

**Goal:** Functions collect extra keyword arguments into a dict.

**Prerequisite:** Phase 1.1 (dict), Phase 6.1 (*args).

**Files:** `python_ifa_build_syms.cc`, `python_ifa_build_if1.cc`.

**Tests to pass:**
```python
def f(**kwargs):
    for k in kwargs:
        print(k, kwargs[k])

f(a=1, b=2)    # a 1 / b 2

def g(x, **kw):
    print(x, kw['y'])

g(10, y=20)    # 10 20
```

---

### Phase 6.3 — Keyword arguments at call sites (~1.5 h)

**Goal:** `f(x=1, y=2)` and `f(*seq)` and `f(**d)` at call sites.

**Files:** `python_ifa_build_if1.cc` (call-site argument marshalling).

**Tests to pass:**
```python
def rect(width, height, filled=False):
    print(width, height, filled)

rect(3, 4)                    # 3 4 False
rect(height=4, width=3)       # 3 4 False
rect(3, height=4, filled=True)  # 3 4 True

def f(*a): print(a)
f(*[1, 2, 3])     # (1, 2, 3)

def g(**kw): print(kw)
g(**{'x': 1})     # {'x': 1}
```

---

## Milestone 7 — OOP Enhancements

### Phase 7.1 — @classmethod and @staticmethod (~1.5 h)

**Goal:** `@classmethod` methods receive the class as first arg; `@staticmethod`
methods receive no implicit first arg.

**Files:** `python_ifa_build_syms.cc` and `python_ifa_build_if1.cc` (detect decorators,
transform call convention). `__pyc__.py` (add `classmethod` and `staticmethod`
as descriptor classes).

**Tests to pass:**
```python
class Counter:
    count = 0
    def __init__(self):
        Counter.count += 1
    @classmethod
    def get_count(cls):
        return cls.count
    @staticmethod
    def description():
        return "a counter"

Counter()
Counter()
print(Counter.get_count())    # 2
print(Counter.description())  # a counter
```

---

### Phase 7.2 — @property (~1.5 h)

**Goal:** `@property`, `@name.setter`, `@name.deleter` descriptors.

**Files:** `python_ifa_build_if1.cc` (recognize `@property` decorator, generate
getter/setter dispatch), `__pyc__.py` (add `property` descriptor class).

**Tests to pass:**
```python
class Circle:
    def __init__(self, radius):
        self._r = radius
    @property
    def radius(self):
        return self._r
    @radius.setter
    def radius(self, value):
        if value < 0:
            raise ValueError("negative radius")
        self._r = value
    @property
    def area(self):
        return 3.14159 * self._r * self._r

c = Circle(5)
print(c.radius)       # 5
c.radius = 10
print(c.radius)       # 10
print(c.area)         # 314.159
```

---

### Phase 7.3 — Multiple inheritance with C3 MRO (~2 h)

**Goal:** `class C(A, B)` where MRO is computed at compile time via C3 linearization.

**Files:** `python_ifa_build_syms.cc` (`gen_class_pyda`: extend to handle multiple bases,
compute C3 MRO, set `inherits` list in order), `python_ifa_build_if1.cc` (method
resolution walks MRO list), `__pyc__.py` (update `super()` to respect MRO).

**Tests to pass:**
```python
class A:
    def method(self):
        return "A"

class B(A):
    def method(self):
        return "B" + super().method()

class C(A):
    def method(self):
        return "C" + super().method()

class D(B, C):
    def method(self):
        return "D" + super().method()

print(D().method())   # DBCA

# Diamond inheritance
class X:
    val = 1
class Y(X):
    pass
class Z(X):
    val = 2
class W(Y, Z):
    pass
print(W().val)        # 1  (Y before Z in MRO)
```

---

### Phase 7.4 — Rich comparisons and __hash__ (~1 h)

**Goal:** Full `__eq__`, `__ne__`, `__lt__`, `__le__`, `__gt__`, `__ge__`, `__hash__`.

**Files:** `__pyc__.py` (add comparison methods to built-in types and as a protocol),
`python_ifa_build_if1.cc` (ensure comparison operators dispatch to dunder methods).

**Tests to pass:**
```python
class Version:
    def __init__(self, major, minor):
        self.major = major
        self.minor = minor
    def __eq__(self, other):
        return self.major == other.major and self.minor == other.minor
    def __lt__(self, other):
        if self.major != other.major:
            return self.major < other.major
        return self.minor < other.minor
    def __le__(self, other):
        return self == other or self < other

v1 = Version(1, 0)
v2 = Version(1, 2)
v3 = Version(1, 0)
print(v1 == v3)       # True
print(v1 < v2)        # True
print(v2 <= v1)       # False
```

---

### Phase 7.5 — User-defined iterators (__iter__ / __next__) (~1 h)

**Goal:** Any class defining `__iter__` and `__next__` works in `for` loops and `next()`.

**Files:** `python_ifa_build_if1.cc` (for-loop code gen: check if iterable has `__iter__`;
call it, then use `__next__` + `StopIteration` sentinel or `__pyc_more__` extension).

**Tests to pass:**
```python
class Countdown:
    def __init__(self, n):
        self.n = n
    def __iter__(self):
        return self
    def __next__(self):
        if self.n <= 0:
            raise StopIteration
        self.n -= 1
        return self.n + 1

for x in Countdown(3):
    print(x)    # 3 2 1

class Squares:
    def __init__(self, limit):
        self.i = 0
        self.limit = limit
    def __iter__(self):
        return self
    def __next__(self):
        if self.i >= self.limit:
            raise StopIteration
        result = self.i * self.i
        self.i += 1
        return result

print(list(Squares(4)))   # [0, 1, 4, 9]
```

---

## Milestone 8 — Starred Assignment and Extended Unpacking

### Phase 8.1 — Starred assignment (a, *b = seq) (~1.5 h)

**Goal:** Extended iterable unpacking in assignment targets.

**Files:** `python_ifa_build_syms.cc` (mark starred targets), `python_ifa_build_if1.cc`
(generate code that slices the iterable at the starred position).

**Tests to pass:**
```python
first, *rest = [1, 2, 3, 4, 5]
print(first)    # 1
print(rest)     # [2, 3, 4, 5]

*init, last = [1, 2, 3, 4, 5]
print(init)     # [1, 2, 3, 4]
print(last)     # 5

a, *b, c = [1, 2, 3, 4, 5]
print(a, b, c)  # 1 [2, 3, 4] 5

# empty star
x, *empty, y = [1, 2]
print(empty)    # []
```

---

### Phase 8.2 — Starred expressions in calls and literals (~1 h)

**Goal:** `f(*seq)`, `[*a, *b]`, `{**d1, **d2}`.

**Prerequisite:** Phase 6.3 (keyword args at call sites), Phase 1.1 (dict).

**Tests to pass:**
```python
def f(a, b, c): return a + b + c
print(f(*[1, 2, 3]))     # 6

a = [1, 2]
b = [3, 4]
print([*a, *b])           # [1, 2, 3, 4]
print((*a, *b))           # (1, 2, 3, 4)

d1 = {'x': 1}
d2 = {'y': 2}
d3 = {**d1, **d2}
print(d3['x'], d3['y'])  # 1 2
```

---

## Milestone 9 — Standard Library Modules

*Each module is a new `.py` file in the compiler's system directory (`__pyc__/`)
that wraps C library calls via `__pyc_c_call__`.*

### Phase 9.1 — math module (~1.5 h)

**Files:** Create `math.py` in the system directory.

**Tests to pass:**
```python
import math
print(math.sqrt(9.0))       # 3.0
print(math.floor(3.7))      # 3
print(math.ceil(3.2))       # 4
print(math.pow(2.0, 10.0))  # 1024.0
print(math.log(math.e))     # 1.0
print(math.log2(8.0))       # 3.0
print(math.pi)              # 3.14159...
print(math.inf > 1e300)     # True
print(math.gcd(12, 8))      # 4
print(math.factorial(5))    # 120
print(math.sin(0.0))        # 0.0
print(math.fabs(-3.5))      # 3.5
```

---

### Phase 9.2 — sys module (~1 h)

**Files:** Create `sys.py` in the system directory; update `pyc.cc` to populate
`sys.argv` from the compiled binary's `main(argc, argv)`.

**Tests to pass:**
```python
import sys
print(type(sys.argv))       # list (or similar)
print(sys.version)          # "pyc ..."
# sys.exit tested via:
# import sys; sys.exit(0)  → program exits 0
```

---

### Phase 9.3 — os.path module (~1.5 h)

**Files:** Create `os.py` and `os/path.py` using `__pyc_c_call__` to wrap POSIX.

**Tests to pass:**
```python
import os
import os.path

cwd = os.getcwd()
print(len(cwd) > 0)                    # True
print(os.path.join('/a', 'b', 'c'))    # /a/b/c
print(os.path.basename('/a/b/c.txt'))  # c.txt
print(os.path.dirname('/a/b/c.txt'))   # /a/b
print(os.path.exists('/'))             # True
print(os.path.exists('/no/such/path')) # False
print(os.path.splitext('foo.txt'))     # ('foo', '.txt')
```

---

### Phase 9.4 — re module (regex) (~2 h)

*PCRE is already linked into pyc binaries.*

**Files:** Create `re.py`; add C glue in `pyc_c_runtime.h` for PCRE match/search/sub.

**Tests to pass:**
```python
import re

m = re.match(r'\d+', '42abc')
print(m.group())         # 42
print(m.start(), m.end())  # 0 2

m2 = re.search(r'\d+', 'abc42def')
print(m2.group())        # 42

print(re.findall(r'\d+', 'a1b22c333'))  # ['1', '22', '333']
print(re.sub(r'\d+', 'X', 'a1b22c'))   # aXbXc

pat = re.compile(r'(\w+)=(\w+)')
m3 = pat.search('key=value')
print(m3.group(1), m3.group(2))  # key value
```

---

### Phase 9.5 — File I/O (~1.5 h)

**Prerequisite:** Phase 2.4 (with/as) for `with open() as f:` idiom.

**Files:** `__pyc__.py` (add `class file` / `open()` built-in using `__pyc_c_call__`
wrapping `fopen`/`fread`/`fwrite`/`fclose`/`fgets`).

**Tests to pass:**
```python
import os

# write then read
with open('/tmp/pyc_test.txt', 'w') as f:
    f.write("hello\n")
    f.write("world\n")

with open('/tmp/pyc_test.txt', 'r') as f:
    for line in f:
        print(line.rstrip())    # hello / world

# read all at once
with open('/tmp/pyc_test.txt') as f:
    contents = f.read()
print(len(contents))            # 12

os.remove('/tmp/pyc_test.txt')
```

---

### Phase 9.6 — collections module (~2 h)

**Files:** Create `collections.py`.

**Features:**
- `namedtuple(name, fields)` — compile-time code generation, returns a new class
- `defaultdict(factory)` — dict subclass with default values
- `Counter(iterable)` — dict subclass counting elements
- `deque([iterable])` — double-ended queue with `appendleft`, `popleft`

**Tests to pass:**
```python
from collections import namedtuple, defaultdict, Counter, deque

Point = namedtuple('Point', ['x', 'y'])
p = Point(1, 2)
print(p.x, p.y)     # 1 2

dd = defaultdict(int)
dd['a'] += 1
dd['a'] += 1
print(dd['a'])      # 2
print(dd['b'])      # 0

c = Counter([1, 2, 2, 3, 3, 3])
print(c[3])         # 3
print(c[1])         # 1

d = deque([1, 2, 3])
d.appendleft(0)
d.append(4)
print(list(d))      # [0, 1, 2, 3, 4]
d.popleft()
d.pop()
print(list(d))      # [1, 2, 3]
```

---

## Milestone 10 — Python 3.6+ Syntax

### Phase 10.1 — Type annotations (~1 h)

**Goal:** Accept PEP 526 / 484 annotations; use them as optional hints to inference.

**Grammar change:** Add annotation syntax (`x: T`, `x: T = v`, `def f(x: T) -> R`).

**Files:** `python.g` (add `:` annotation in param and variable positions),
`python_ast.h` (add `PY_annotation`), regenerate `python.g.d_parser.cc`,
`build_syms_pyda` / `build_if1_pyda` (ignore annotations by default, or use as type
hints to narrow inference).

**Tests to pass:**
```python
def add(x: int, y: int) -> int:
    return x + y

print(add(1, 2))    # 3

def greet(name: str) -> str:
    return "hello " + name

print(greet("pyc"))  # hello pyc

count: int = 0
count += 1
print(count)        # 1
```

---

### Phase 10.2 — Walrus operator := (~1 h)

**Grammar change:** Add `:=` as a named expression token.

**Files:** `python.g`, `python_ast.h` (add `PY_walrus`), regenerate parser,
`build_syms_pyda`, `build_if1_pyda`.

**Tests to pass:**
```python
data = [1, 3, 5, 7, 9]
if (n := len(data)) > 3:
    print(n)    # 5

# walrus in while
import re
text = "foo123bar456"
pos = 0
while m := re.search(r'\d+', text[pos:]):
    print(m.group())    # 123 / 456
    pos += m.end()

# walrus in list comprehension
nums = [1, -2, 3, -4, 5]
pos_squares = [y for x in nums if (y := x * x) > 4]
print(pos_squares)      # [4, 9, 16, 25]  (or similar based on semantics)
```

---

## Milestone 11 — Developer Experience

### Phase 11.1 — Improved diagnostics (~1 h)

**Goal:** Better error messages for common mistakes.

**Files:** `python_ifa_sym.cc`, `python_ifa_build_if1.cc`, `python_ifa_main.cc`.

**Improvements:**
- Type error messages include concrete inferred types ("cannot add `int` and `str`")
- Unresolved call errors show which overloads exist
- File/line info for all error messages (already tracked in `PycAST`)
- "did you mean X?" for near-miss symbol names

---

### Phase 11.2 — assert statement (~0.5 h)

*Already in grammar; needs a one-case compiler fix.*

**Files:** `python_ifa_build_if1.cc` (add `PY_assert_stmt` case: lower to
`if not expr: raise AssertionError(msg)`).

**Prerequisite:** Phase 2.2 (raise).

**Tests to pass:**
```python
assert True
assert 1 + 1 == 2, "math broken"

try:
    assert 1 == 2, "one != two"
except AssertionError as e:
    print("caught assert")   # caught assert
```

---

### Phase 11.3 — __repr__ / __str__ integration (~1 h)

**Goal:** `repr(x)` and `str(x)` call `__repr__` / `__str__`; `print()` uses `__str__`.

**Files:** `__pyc__.py` (add `repr()` built-in; improve `str()` to call `__str__`).
`python_ifa_build_if1.cc` (ensure print uses `__str__` dispatch).

**Tests to pass:**
```python
class Point:
    def __init__(self, x, y):
        self.x = x
        self.y = y
    def __repr__(self):
        return "Point(" + str(self.x) + ", " + str(self.y) + ")"
    def __str__(self):
        return "(" + str(self.x) + ", " + str(self.y) + ")"

p = Point(1, 2)
print(p)          # (1, 2)
print(repr(p))    # Point(1, 2)

pts = [Point(0, 0), Point(1, 2)]
# print(pts)  -- list __str__ calls repr() on each element
```

---

### Phase 11.4 — Separate compilation / caching (~2 h)

**Goal:** Modules that haven't changed are not re-parsed and re-compiled. Emit per-module
`.o` files; link only changed modules.

**Files:** `pyc.cc` (add content-hash check for each imported module), `Makefile`
(add `INCREMENTAL=1` mode).

**Implementation note:** Compute SHA-256 of each source file; if a cached `.o` exists
for the same hash and the module's exported type interface hasn't changed, skip
re-compilation. This is primarily useful for large projects with many stable library
modules.

---

## Appendix — Feature → File Map

| Feature area | Primary files |
|---|---|
| Grammar additions | `python.g` → regenerate `python.g.d_parser.cc` |
| New AST node kinds | `python_ast.h` |
| Symbol-table pass | `python_ifa_build_syms.cc` |
| Code-generation pass | `python_ifa_build_if1.cc` |
| Scope mechanics | `python_ifa_sym.cc` |
| Built-in types / functions | `__pyc__.py` |
| C runtime primitives | `pyc_c_runtime.h` |
| Standard library modules | new `<name>.py` in system dir |

### Grammar change process

1. Edit `python.g`
2. Run `make python.g.d_parser.cc` (calls `make_dparser` via the Makefile rule)
3. The regenerated file is large (~45 k states) and slow to compile with `-O3`;
   use `DEBUG=1` during development
4. Add new `PY_xxx` enum values to `python_ast.h` and handle in both `build_syms_pyda`
   and `build_if1_pyda`

---

## Phase Dependency Graph

```
Milestone 1 (Dict/Set/Comprehensions)
  1.1 Dict ─────────────────────────────────────┐
  1.2 Set ──────────────────────────────────────┤
  1.3 Dict/Set/Gen comprehensions               │
                                                │
Milestone 2 (Exceptions)                        │
  2.1 Runtime infrastructure                    │
   └─ 2.2 try/except/raise ──────────────────── │ ─┐
       └─ 2.3 try/except/else/finally ──────────│──┤
           └─ 2.4 with/as ──────────────────────│──┤
                                                │  │
Milestone 3 (Generators)                        │  │
  3.1 yield ─────────────────────────────────── │──┤
   └─ 3.2 yield from / genexpr ──────────────── │  │
                                                │  │
Milestone 4 (Strings)                           │  │
  4.1 String methods                            │  │
  4.2 str.format() / f-strings (grammar change) │  │
                                                │  │
Milestone 5 (List/Builtins)                     │  │
  5.1 List methods                              │  │
  5.2 enumerate/zip/map/filter/reversed/sorted ─┘  │
  5.3 sum/min/max/list()/tuple()/set()             │
                                                   │
Milestone 6 (*args/**kwargs)                       │
  6.1 *args ────────────────────────────────────── │ ─┐
  6.2 **kwargs ─────────────────────────────── 1.1─┘  │
  6.3 Keyword args at call sites                      │
                                                      │
Milestone 7 (OOP)                                     │
  7.1 @classmethod / @staticmethod                    │
  7.2 @property                                       │
  7.3 Multiple inheritance ────────────────────────── │
  7.4 Rich comparisons                                │
  7.5 User-defined __iter__ / __next__ ─────── 2.2 ──┘

Milestone 8 (Starred assignment)
  8.1 a, *b = seq
  8.2 *seq in call/literal ──────────────────── 6.3

Milestone 9 (Standard Library)
  9.1 math                                     (independent)
  9.2 sys                                      (independent)
  9.3 os.path                                  (independent)
  9.4 re ──────────────────────────────────── 2.2 (exceptions)
  9.5 File I/O ────────────────────────────── 2.4 (with/as)
  9.6 collections ─────────────────────────── 1.1 (dict)

Milestone 10 (Python 3.6+ syntax)
  10.1 Type annotations (grammar change)
  10.2 Walrus := (grammar change)

Milestone 11 (DX)
  11.1 Diagnostics                            (independent)
  11.2 assert ─────────────────────────────── 2.2
  11.3 __repr__ / __str__ integration
  11.4 Separate compilation
```
