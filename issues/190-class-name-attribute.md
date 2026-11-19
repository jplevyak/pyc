# 190 — a class has no `__name__`

**Status:** open. Split from [184](closed/184-classmethod-attribute-through-cls.md)
on 2026-11-18, when the rest of 184 was fixed. A refusal, not a wrong answer.

## Symptom

```python
class C:
    @classmethod
    def name(cls):
        return cls.__name__
print(C.__name__)   # CPython: C.   pyc: unresolved member '__name__' of class 'C'
print(C.name())     # same refusal, through `cls`
```

`type(x).__name__` is refused earlier, because `type()` is not supported.

## Cause

Nothing defines it. A class's attributes are its prototype's fields
(`Sym::self`), and `__name__` is not one. Since 184, a read through `cls`
reaches the prototype the same way `C.attr` does, so whatever supplies
`C.__name__` serves `cls.__name__` too.

## Fix (to choose)

- Install `__name__` as a prototype field in `gen_class_pyda`, as a
  class-body assignment would. Simple, but instances copy class attributes
  (issues/173 item 1), so every instance gets a slot that slot elision must
  remove, `inst.__name__` reads the name where CPython raises
  `AttributeError`, and the codegen goldens change.
- Answer it as a constant: the frontend for `C.__name__`, and FA (through
  `IFACallbacks`, since it is a Python name) for a period on a meta-typed
  receiver. No layout change, but one more special selector.

## Verification

`tests/class_name_attribute.py` (`.known_issue`) flips to PASS on both
backends.
