# 196 — compiling shedskin with pyc

**Status:** open, tracking. Filed 2026-11-19. This lists what stands between
pyc and compiling shedskin's own compiler source
(`../shedskin/shedskin`, 7aef6e02, about 16.7k lines in its top-level
modules and `state/`), measured with a driver that calls
`Shedskin.commandline()`.

The first three blockers are fixed: [191](closed/191-base-class-named-through-a-module.md),
[192](closed/192-multi-line-parenthesized-import.md) and
[193](closed/193-zero-argument-super.md). `typing` and `@dataclass` landed
in [195](195-typing-and-dataclasses.md). The next stop is `typing.NamedTuple`
(`python.py:560`, below). With that class replaced by hand with the plain
class a lowering would produce, pyc stops within 3 seconds at `log.py:11`,
`class ShedskinFormatter(logging.Formatter)`, because there is no `logging`.

**This is an inventory, not a compile trace.** It comes from reading the
imports and grepping for features pyc is known to refuse, so it is a floor.
More is likely behind it, and whether ifa can type the program at all
(item 5) is unknown until everything else is through.

## 1. The `ast` module (decides feasibility)

shedskin is a Python compiler, and it parses with CPython's `ast` module,
which CPython implements in C: `python.py:376` returns
`ast.parse(filebuf)`. Twelve modules import `ast`, and everything after the
parse walks CPython's node classes (about 500 `isinstance` calls). pyc would
need:

- the node classes with CPython's exact fields and `_fields` (around 100);
- a parser producing them, either by exposing pyc's own C++ parser (DParser
  with `python.g`, whose AST is not CPython's) or with a parser written in
  Python;
- `ast.iter_fields`, a generic read of a node's fields by name, used by
  `BaseNodeVisitor.generic_visit` (`ast_utils.py:173`);
- `ast.unparse` (for example `infer.py:1965`) and `ast.walk`.

This is larger than everything else here put together.

## 2. Visitor dispatch by computed name (decides feasibility)

`BaseNodeVisitor.visit` (`ast_utils.py:160`) finds each node's handler like
this:

```python
method = "visit_" + node.__class__.__name__
visitor = getattr(self, method, None)
```

pyc refuses both parts: `getattr` with "builtin 'getattr' is not supported
by pyc", and `x.__class__` (see also [190](190-class-name-attribute.md) for
`__name__`). A `getattr` whose name is a run-time string is a lookup of an
attribute from a string at run time. Two answers fit pyc's contract:

- support `getattr`/`hasattr` with a computed string on a receiver whose
  classes are known, compiled to a table of each class's attributes.
  Implementing CPython's own by-name lookup is not analysis by name. The
  table's result type is the union of the matching attributes, so
  visitors whose `visit_*` methods share a signature type cleanly;
- refuse it, which stops shedskin here.

There are also constant-string uses (`getattr(node, "level", None)`,
`getattr(self.options, "local_deps", False)`, 8 `hasattr` calls) and
`type(...)` / `__class__.__name__` elsewhere (26 and 18 matches). The
constant-string forms are easy: they are a member read, with the default
used when the class has no such member.

## 3. Missing standard-library modules

| module | imported by | note |
| --- | --- | --- |
| `ast` | 12 modules | item 1 |
| `logging` | `__init__`, `log`, `error`, `infer`, `typestr`, `cmake`, `extmod` | **done 2026-11-19** ([197](197-logging.md)); `logging.Formatter` subclassed |
| `pathlib` | 10 modules incl. `python`, `cpp`, `graph`, `state/*` | **done 2026-11-20** ([202](202-os-and-pathlib.md)), with a real `os.path` |
| `argparse` | `__init__`, `config`, `cmake` | the command line; **not typeable as is** ([206](206-argparse.md)): its Namespace is named and typed at run time |
| `subprocess`, `platform` | `__init__`, `config`, `makefile`, `cmake` | |
| `shutil`, `glob`, `zipfile`, `sysconfig` | `makefile`, `cmake` | build-system generation only |
| `sqlite3`, `tokenize` | `stats` | statistics only |
| `textwrap` | `cpp` | **done 2026-11-20** ([205](205-textwrap.md)) |
| `importlib` | `python` | |

`typing`, `dataclasses`, `sys`, `os`, `re`, `collections`, `time`, `io`,
`struct`, `string`, `random`, `math`, `itertools`, `functools` and `copy`
have pyc stubs. Those stubs probably lack some functions shedskin calls;
not checked yet.

A driver that only translates `.py` to `.cpp` could leave out the
build-system and statistics modules (`makefile`, `cmake`, `stats`), but
`__init__.py` imports all of them at the top. That would be a corpus-style
edit to shedskin, and `logging` and `pathlib` would still be needed.

## 4. `typing.NamedTuple`

One use: `VarLookup(var, is_global)`, `python.py:560`. Its five
constructions are in `smart_lookup_var`, and its two callers
(`python.py:554`, `cpp.py:4436`) only test it for truth and read `.var` and
`.is_global`. A lowering to an immutable class (the frozen-dataclass
machinery of 195) serves it exactly, provided every use of the instance as a
tuple is refused: indexing, unpacking, `len`, iteration, `==` against a
plain tuple, `hash` agreeing with the plain tuple, and
`isinstance(p, tuple)`. The last two compile silently unless refused
explicitly.

A NamedTuple that is a real tuple needs subclassing of built-in containers,
which pyc does not have at all: `class P(tuple)` and `class P(list)` both
fail at construction ("illegal call argument type 'P'"). A tuple is made by
the `make`/`make_seq` primitive on `sym_tuple` (`python_ifa_build_if1.cc`
around line 1159); a user class by cloning its prototype and calling
`__init__` (`python_ifa_build_syms.cc` around line 3140). A subclass takes
the second path and so has neither a constructor taking the iterable nor
any elements. Fixing that is ifa work: a sequence CreationSet that carries a
class, with codegen giving it tuple layout and the class's methods.

## 5. The analysis itself (unknown)

16.7k lines of AST-walking code with 62 lambdas and 59 generators. The node
classes share the `ast.AST` base, so a field holding many node kinds is
polymorphism under one user-visible base (hoisted, not split), not a union
pyc must refuse. Nothing can be measured here until items 1 to 3 are done.

## Order

Items 1 and 2 decide whether this is worth doing. If both are, the order is
2 (smallest and general), the cheap parts of 3 (`logging`, `pathlib`,
`argparse`, `textwrap`), 4, then 1.
