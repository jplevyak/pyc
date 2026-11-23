# 209 — `obj.m(*t)` of a float tuple into a method that stores its arguments emits an invalid cast

**Status:** open. Found 2026-11-22 while landing enum (issues/208), whose
lowering now passes a tuple display's elements by index and so avoids it.
A refusal at the C/LLVM level, not a wrong answer.

## Symptom

```python
class P:
    def g(self, a, b):
        self.a = a
        self.b = b
t = (3.5, 2.5)
p = P()
p.g(*t)
print(p.a, p.b)       # CPython: 3.5 2.5
```

| backend | pyc |
| --- | --- |
| C | `cannot cast from type 'double' to pointer type '_CG_nil_type'` (clang) |
| LLVM | `LLVM module verification failed: Invalid bitcast %10 = bitcast double %9 to ptr` |

It needs all three conditions together:

- `t` holds floats: `(3, 2)` works;
- the call star-unpacks: `p.g(t[0], t[1])` works;
- the method returns None: a `g` that returns `b` works.

`f(*t)` of a plain function works.

The emitted C shows the shape: a setter's result temp is typed `_CG_nil_type`
and assigned the stored float, `t129 = (_CG_nil_type)2439700.0;`. Some
result of the inlined/unpacked call is typed by the method's None return
where it holds an argument value.

## Next

Find which Var gets the None type: the star-unpack path (issues/189's
arity binding) against the setter's result Var. Compare with the
`p.g(t[0], t[1])` IR.
