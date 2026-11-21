# 206 — `argparse`: its Namespace cannot be typed statically

**Status:** open, a design decision. Filed 2026-11-20 when `argparse` was
next in [196](196-compiling-shedskin-with-pyc.md)'s order. Nothing is
implemented: `import argparse` is a compile error (no such module).

## The obstacle

The parser is ordinary code; the result is not. After

```python
p = argparse.ArgumentParser()
p.add_argument("--int32", action="store_true")
p.add_argument("-o", "--outputdir")
p.add_argument("-d", "--debug", type=int)
p.add_argument("-I", "--include-dirs", action="append")
args = p.parse_args()
```

`args.int32` is a bool, `args.outputdir` a str or None, `args.debug` an
int or None, `args.include_dirs` a list or None. The attribute NAMES are
computed at run time from the option strings (`"--include-dirs"` becomes
`include_dirs` through `lstrip("-").replace("-", "_")`), and CPython
creates them with `setattr(namespace, action.dest, value)`.

pyc's objects have a fixed set of fields, each of one type, known at
compile time. Here:

- the field names exist only as run-time strings, so the class cannot be
  built from the program text (and `__getattr__` is not supported: a
  read of an undeclared field is an unresolved member);
- even with a by-name table (196 item 2's proposal for `getattr`), every
  `setattr(ns, dest, value)` writes the value of EVERY action to the table,
  so each field's type is the union of all of them, `{bool, str, int,
  list, None}`, which has no representation without boxing.

What would tie a name to its type is the add_argument call that made it,
and that link passes through a list of Action objects and string
operations. Separating it again would be partial evaluation of the
parser's construction, which pyc does not do and the provenance rule
(AGENTS.md) argues against doing by special case.

shedskin's own `commandline()` mixes all four kinds, plus subparsers,
`parents=` and `append`.

## Options

1. **Refuse it** (today), with a better message than "cannot find
   module": point to `getopt` (in `pyc_lib`, typed: `(opt, value)` str
   pairs) and `sys.argv`.
2. **The one-type subset.** Support `__getattr__` in the frontend (a read
   of a field the class does not have becomes `x.__getattr__("name")`,
   a general CPython feature), back `Namespace` by a dict, and let a
   program whose arguments all produce one type compile; any mix is the
   ordinary refusal of a union with no representation. Honest, but nearly
   every real command line mixes flags (bool) with options (str), so it
   would rarely compile.
3. **Compile-time evaluation of the parser.** When every `add_argument`
   call's arguments are constants and the parser is built at module level
   or in one function called once, evaluate the construction at compile
   time and synthesize a Namespace class with one typed field per dest.
   This is what a typed argparse needs; it is a frontend partial
   evaluator, large, and limited to parsers built from constants.
4. **Edit the program**, as the corpus policy allows for a real type
   error: not applicable, this is valid Python.

For shedskin itself, 196 then needs option 3 or a hand-written
replacement of `commandline()` in a driver.

## Verification

None yet: depends on the option chosen.
