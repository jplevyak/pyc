# 173 — silent wrong answers the strict-suite comparison found

**Status:** open on three items. Filed 2026-09-29 from
[171](closed/171-permissive-accommodations-must-be-flagged-and-non-strict.md)'s
verification: every `tests/*.py` compiled with `--strict`, run, and compared
with CPython. Its first item, `isinstance` on a `{list, str}` value folding
to False, was fixed 2026-10-15 by refusing what no run-time tag can decide
(`cg_isinstance_tag_targets`; `tests/isinstance_dynamic.py.check_fail`).
Each item below is wrong the same way in every mode, so none is an
accommodation: each needs a fix or a refusal.

## 1. A class attribute read through a subclass or an instance after the class is mutated

`tests/class_attr_mutation.py` (`.known_issue`; its check holds CPython's
output):

```python
class A: n = 2
class B(A): o = 3
A.n = 4
print(B.n)          # CPython: 4.   pyc: 2
```

**Wider than filed (measured 2026-11-14).** Instances have the same
defect, and that is the common case:

```python
class A: n = 2
a = A()
A.n = 9
print(a.n)          # CPython: 9.   pyc: 2

class C:
    k = 0
    def bump(self):
        C.k += 1
        return self.k
c = C()
print(c.bump(), c.bump())   # CPython: 1 2.   pyc: 0 0
```

`tests/class_attr_mutation_instances.py` (`.known_issue`) holds these.

**Mechanism.** A class is a struct type with a prototype object. A
subclass's prototype copies each inherited member when the class is
created (`python_ifa_build_syms.cc`, the base `___init___`), and an
instance is a clone of its class's prototype. Every class attribute is
therefore copied at the moment the subclass or the instance is made.
CPython looks an attribute up at READ time: the instance, then the class
MRO. The two agree exactly when a class attribute is never stored
through a class after something has copied it.

**Why the filed fix is not enough.** "A subclass does not copy an
inherited attribute it never assigns; its read goes to the base's
prototype" fixes `B.n` but not `B().n` (the instance clones B's
prototype) nor any instance read. Any fix has to change what a read of
such a member means. It is a representation change, not a lowering fix.

**Two designs, neither started:**

- *A shared cell per mutated class attribute.* The prototype holds a
  pointer to a cell `{owner, value}`, and subclass prototypes and
  instances copy the pointer, so they alias the class's cell. A read
  dereferences it. A store through an object that owns the cell mutates
  it in place (`A.n = 4`, seen by B and every instance). A store through
  one that does not (`B.n = 5`, `a.n = 1`) installs a fresh cell it
  owns. That is CPython's lookup for every case above, at one
  indirection, paid only by the members that need it.
- *Redirect the read.* A read of a member that no store ever reaches
  through an instance goes to the defining class's prototype. Cheaper,
  but it covers instances and not a subclass that later assigns its own
  value (`B.n = 5`, read before that store), and "no store reaches it
  through an instance" has to come from FA's types, not from the
  attribute's name (AGENTS.md).

**Which members.** Only a class attribute stored through a class value
after definition needs either design, and that has to be decided
structurally: a `setter` whose receiver can be a class prototype
CreationSet, found by FA, not a scan for `Name.attr =` in the source.
Everything else keeps the copy, which is exact for it.

**Corpus: no witness.** The two programs that mutate class attributes
(`chull`'s `Edge.enumm += 1`, `bh`'s `BH.nbody = ...`) read them back
through the class itself, which is correct today. Priority is the suite
and real programs: the class-counter idiom read through `self` is
common Python. The same gap refuses a decorator stored on a class after
its definition ([007](closed/007-decorators-not-applied.md)).

## 2. A global read before its definition sees the later value

`tests/scope_global_before_define.py` (run under `--test-scoping`, so the
binary never runs):

```python
class a:
  global z
  print(z)          # CPython: NameError.   pyc: prints 1
z = 1
```

A read CPython rejects at run time must be refused or checked at run
time, as closed/103 does for an executed undefined name; it must not see
a value from the future. The flow-insensitive global slot is
[ifa/050](../ifa/issues/050-FA-general-constant-propagation-unreachable-code.md)'s
subject; a write-once global (its 1b) answers this case exactly.

## 3. Deep recursion overflows the C stack; `sys.setrecursionlimit` is ignored

`tests/list_index_type_mismatch_salvage.py` (corpus `loop`) segfaults in
every mode: 16,883 frames of DFS recursion, and `ulimit -s unlimited`
runs it to completion. The program calls `sys.setrecursionlimit(100000)`,
which `pyc_lib/sys.py` accepts and ignores. Fix: run the program on a
stack sized to the limit (the runtime's `main` starts the program on a
thread with a large stack), and turn an overflow into CPython's
`RecursionError` rather than a SIGSEGV.

## Noted, not actionable here

- `lambda_class_attr`: the default instance repr is `<object>` where
  CPython prints `<__main__.A object at 0x…>`. The address can never
  match; printing the class name is cheap and is all that can.
- `zip_builtin`, `tuple_list_mix`: `len(zip(...))` and `zip(...)[i]`
  compile where CPython raises. The eager lists are a documented
  representation by the author's decision (README accommodation table).
- `list_tuple_union_method`: a `{list, tuple}` receiver,
  [ifa/102](../ifa/issues/closed/102-codegen-detected-conditions-are-refusals-or-demands.md).

## Verification

Re-run the comparison (the script is in 171's verification record):
every item matches CPython or is refused with a named diagnostic.
