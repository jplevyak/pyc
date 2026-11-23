# 208 — the `enum` module

**Status:** open. Stage 1 (`Enum`, `auto`, `unique`) landed 2026-11-22.
Not started: `IntEnum` / `StrEnum` (they need an int/str subclass that
carries attributes) and `Flag` / `IntFlag`.

## Symptom

`from enum import Enum` failed at the first class:
`base 'Enum' of class 'Color' is not a class`. `enum` is one of the most
imported stdlib modules in real code, though nothing in the corpus or the
suite used it.

## What it needed

The VALUE side of an enum was already expressible. A class whose members
are class attributes holding its own instances, with `name`/`value`,
identity, hashing and `str`/`repr`, compiled and matched CPython when
written out by hand. What was missing is what CPython's metaclass
(`EnumType`) does on the CLASS:

| operation | before |
| --- | --- |
| `for m in Color`, `list(Color)`, `len(Color)`, `x in Color` | `unresolved call '__iter__'` |
| `Color(1)` (lookup by value) | no way to replace the constructor |
| `Color['RED']` | same family |
| `metaclass=` | a parse error |

## Design

Three general mechanisms, then a lowering.

1. **`@__pyc_metamethod__`** (`python_ifa_build_syms.cc`). A method whose
   receiver formal is the class's META type, so its receiver is the class
   value. It is not a member of the class (no prototype slot, no instance
   method). ifa makes a subclass's meta type a subtype of its base's
   (`build_type_hierarchy`), so a metamethod is inherited, as a metaclass
   method is. `for m in C` and `C[k]` are selector sends and dispatch to it
   directly.

2. **A meta `__call__`** takes the shape of the `__new__` wrapper (its first
   formal is the callee), so a call of the class dispatches to it, as to a
   metaclass `__call__`. A class with one in its MRO gets no `__new__`
   wrapper (`gen_class_pyda`), which would otherwise be a second, more
   specific candidate.

3. **`__pyc_new_instance__(C)`** (`python_ifa_build_if1.cc`): the prototype
   clone that `C`'s `__new__` wrapper starts from, without calling `C`.
   Library code whose class replaces its own call uses it to make
   instances.

Two fixes in ifa followed from these.

- **FA: a meta method answers before the prototype**
  (`fa_prims.cc`, `P_prim_period`). `len(x)` is `x.__len__()`, an attribute
  read and then a call. For a class value, issues/184's read found the
  prototype's `__len__`, which pyc's `object` defines as a fallback and
  CPython's `object` does not. In CPython `len(C)` looks `__len__` up on
  `type(C)`. Now a function on the selector whose receiver is a meta type
  the class value has is left to the selector dispatch, and the prototype is
  not read. Found structurally: such a function is a specializer of its
  selector symbol.
- **Codegen: an attribute read through a class value that does not fold**
  (`cg.cc`, `cg_emit_llvm.cc`, `cg_meta_proto_var`). issues/184's reads were
  only ever constant-folded. `len(cls.items)` on a run-time list, in an
  ordinary `@classmethod`, failed in codegen
  (`the attribute 'items' is not a field of the receiver's type 'B'`) on
  both backends. FA gives each class its own contour, so the receiver is
  one class and the read is a field of its prototype global. This was a
  pre-existing bug, independent of enum.

The lowering (`python_ifa_dataclass.cc`, beside the dataclass one, sharing
its import resolution) rewrites a module-level class whose base is an enum
class into what `EnumType` builds. Its header comment and
`pyc_lib/enum.py`'s say exactly what it emits. Membership follows
CPython's `_EnumDict` rule (not a `_sunder_`, `__dunder__` or `__private`
name, not a function). A value equal to an earlier member's makes an alias,
decided at run time by `==` as CPython decides it (`__pyc_enum_add__`).
`auto()` is `_generate_next_value_`'s answer. `PYC_DBG_ENUM=1` prints the
generated code.

## Refused (compile errors naming this issue)

- `IntEnum`, `StrEnum`. A member IS an int or a str carrying a name, and
  pyc has no subclass of int or str that carries attributes: `class P(int)`
  compiles `P(3)` to a plain `int`, and `P(3).f()` is unresolved. Faking
  the member as a record with forwarding dunders would make
  `isinstance(Prio.LOW, int)` False, a silent deviation. So these wait for
  that representation;
- `Flag`, `IntFlag`;
- the functional API, `Enum('Color', 'RED GREEN')`;
- `__new__` in an enum class, `_ignore_`;
- an enum class that is not at module level, or extends an enum with
  members, or carries a decorator other than `@unique`;
- `from enum import *`;
- a subclass of an enum class that the lowering cannot see: one imported
  from another module, or reached through a name the imports do not bind.
  It would be left a plain class, its members raw values. `pyc_enum_check`,
  after build_syms, refuses such a class when it defines a would-be member.

## Known gaps through other issues

- `Color['MISSING']` raises KeyError, but past its `try` (issues/175: no
  exception check follows a subscript send). The metamethod tests
  membership first, because a dict read of a missing key does not raise at
  all (also 175).
- `str.format` of a member and `format()` are not supported for any object.
  `f"{m}"` and `"%s" % m` work.
- An enum's `__init__` on a tuple value is called with the elements by
  index (the tuple display's length is known). `m.__init__(*value)` hit
  issues/209.

## Verification

`tests/enum_basic.py` and `tests/enum_init_methods.py` match CPython's
output on both backends and under `--strict`. `tests/enum_intenum_refused.py`
is the refusal. `tests/classmethod_cls_attribute_runtime.py` covers the
codegen fix.

Corpus: `check__default__b4218032+d403b5ac` is identical to the baseline
`check__default__a3d11f7f+a50dc85c`: 0 compile or run failures, othello2
the one stdout difference (177), CreationSets/shapes 3082/878. The new FA,
codegen and class paths act only when a metamethod or a meta `__call__`
exists, and no corpus program has one. `make test`: 593 passed, 0 failed,
18 known (unchanged), both backends.
