# 195 — `typing` and `dataclasses`

**Status:** open for its residuals. `typing`, `__future__` and `@dataclass`
landed 2026-11-19. Found compiling shedskin's own source, which imports
`typing` in every module and keeps its compiler state in dataclasses
(`state/*.py`). Its next stop is `typing.NamedTuple` (below).

## typing

pyc never evaluates an annotation, so `pyc_lib/typing.py` only binds names:
`TYPE_CHECKING = False`, a `_SpecialForm` value for `Any`, `Optional`,
`Union`, `Dict`, `List`, ... whose subscript is itself (so `Pathlike =
Union[Path, str]` compiles), `TypeVar`, `cast`, `overload`, `final`.
`pyc_lib/__future__.py` binds the feature names; `annotations` is already
pyc's behaviour.

Two statements are removed on the AST (`python_ifa_dataclass.cc`,
`lower_typing`), each matched through the module's import of `typing`:

| statement | why |
| --- | --- |
| the body of `if TYPE_CHECKING:` (an `else` stays) | CPython never runs it; pyc resolves imports at compile time, so an import in it was loaded (and usually not found) |
| `X: TypeAlias = <type>` | a type expression pyc cannot type (`int \| str`, `tuple[int, str]`), which the program names only in annotations |

## dataclasses

`lower_typing_and_dataclasses` rewrites a module-level `@dataclass` class
before `build_syms`, as the ctypes lowering does (issues/188):

- **Defaults are evaluated once, in the class body.** Each goes into a
  module global the class body declares (`global __pyc_dc_K_f__`), and both
  the class attribute and the `__init__` default read it. So they are one
  object, as in CPython, and class-scope names stay visible to the
  expression. (pyc cannot read a class attribute from a method's default,
  `def __init__(self, b=b)`, which is what made this necessary.)
- **`field(default_factory=F)`.** F is stored the same way, and the
  `__init__` default is flagged `PyDAST::is_default_factory`.
  `PycCompiler::default_wrapper`, which supplies every omitted argument,
  CALLS a flagged default instead of passing it. That is CPython's
  semantics exactly, including an explicit `None`. A sentinel default
  (`x if x is not MISSING else F()`) was tried first and does not type: FA
  does not fold `is` on a singleton's type, so the field held the union.
- **Generated methods**, each only if the class body does not define it:
  - `__init__`: fields in order, the dataclass base's first; `init=False`
    fields set from their default or factory; `__post_init__` called if it
    or a dataclass base defines one.
  - `__repr__` in CPython's format (`field(repr=False)` left out).
  - `__eq__`: the fields as a tuple, when the classes are the SAME class.
    pyc has no `type(x)` or `x.__class__` comparison (both fail), so every
    dataclass answers `__pyc_dc_tag__()` with its own number.
  - `__hash__`: CPython's table. With `eq` and not frozen it raises
    `TypeError: unhashable type`; frozen, it hashes the fields.
- **`frozen=True`.** A store to a field is the setter call
  `__pyc_set_FIELD__`, the mechanism ctypes Structures and `@property`
  setters use, and it raises `FrozenInstanceError`. The generated
  `__init__`'s own stores are marked `is_raw_field`, as CPython's go
  through `object.__setattr__`.

**After `build_syms`** (`pyc_dataclass_check`), what only the class
hierarchy shows:

- a subclass of a dataclass must be one itself, since it would inherit a
  `__repr__` that prints the base's name and an `__eq__` that compares the
  base's tag (CPython uses the instance's class for both);
- a dataclass's dataclass base must be the one it was lowered with, which
  is one in the same module;
- a `__post_init__` inherited from a class that is not a dataclass is
  refused, since it would not be called.

Writing the tests found and fixed a general bug. A bare annotation (`x:
int`) in a class body made `x` a member with no value. A subclass reading
the field its own `__init__` sets then failed with "unresolved member 'x'
of class 'A'". In CPython it binds nothing outside a function; inside one
it makes the name local, and still does
(`python_ifa_build_syms.cc`, `PY_annassign`).

## Refused (a compile error naming it)

`order=True`, `unsafe_hash=True`, `kw_only`, `slots`, `weakref_slot`;
`field(kw_only=...)`, `field(hash=<not None>)`; `InitVar`, `KW_ONLY`;
`fields()`, `asdict()`, `astuple()`, `replace()`, `make_dataclass()` (not in
the stub, so refused at the import); a dataclass not at module level; more
than one dataclass base, or one in another module; a list, dict or set
literal as a default (CPython's `ValueError`); a non-default field after a
default one (CPython's `TypeError`); `from typing import *` and `from
dataclasses import *`.

## Residuals

- **`typing.NamedTuple`** (and `TypedDict`, `Generic`, `Protocol`) are not
  in the stub. A NamedTuple that is a real tuple needs ifa work: a tuple
  CreationSet that carries its class, with field names aliasing positions.
  A class with attributes only would be smaller, but `v == (x, y)`,
  `isinstance(v, tuple)` and `hash(v)` would then have to be refused one by
  one.
- **A store to a NON-field attribute of a frozen instance** succeeds; CPython
  raises `FrozenInstanceError` for any name. Only field names have setters.
- **`__eq__` against an object of another class returns False.** CPython
  returns `NotImplemented` and then tries the other operand's `__eq__`, which
  matters only if that class claims equality with the dataclass. pyc cannot
  return `NotImplemented` from `__eq__` (it fails in C codegen,
  `_CG_unknown_type`).
- **`__repr__` does not guard recursion** (CPython prints `...` for a
  dataclass that contains itself); pyc's recurses.
- An empty set field prints as `{}`: [194](194-repr-of-an-empty-set.md).

## Tests

`tests/dataclass_basic.py`, `tests/dataclass_frozen_inherit.py`,
`tests/class_annotation_only.py` match CPython on both backends and under
`--strict`. `tests/dataclass_subclass_refused.py` and
`tests/dataclass_mutable_default_refused.py` check the refusal messages.

Corpus: `check__default__d3c93d16+35a20895` matches
`check__default__9b95fc1a+c933c9ae` row for row (compile verdicts,
warnings, stdout, every EntrySet and CreationSet count), except a chaos run
timeout on a loaded machine; alone it runs in 81 s.
