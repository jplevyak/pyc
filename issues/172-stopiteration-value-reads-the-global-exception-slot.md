# 172 — `StopIteration.value` reads the one global exception slot

**Status:** open. The other half of this issue, a generator's `send`
channel typed as `int64`, was fixed 2026-10-16 (a per-generator `sendval`
cell; `tests/generator_send_typed.py`), with one named accommodation left:
the priming `next()` of the `next(g); g.send(5)` idiom makes the cell
`{None, scalar}`, and under permissive only (`PYC_SEND_NONE_ZERO`, warns at
the `def`) a non-priming `next()` then delivers a zero of the cell's type;
`--strict` refuses. The precise answer is a started/unstarted typestate on
the generator object. The record, and a latent exception-check fold bug
fixed on the way, are in git
(`git show 23cd789:issues/172-generator-send-channel-and-stopiteration-value.md`).

## Symptom

`except StopIteration as e: e.value` reads `__pyc_exc__`, which unions
every `StopIteration` raised anywhere in the program. With two value
types it is refused: `tests/stopiteration_value_exception_slot.py`
(`.known_issue`) holds `{None, int64, str}`. Refused, not silent.

## Mechanism

The CreationSets are already separated by value type; the confluence is
the slot, a single global. `yield from` is unaffected: it reads the
sub-generator's own `__pyc_return_value__()`.

## Fix

Exception flow along the call graph: which raises can reach which
handler, so a handler reads the union of the raises that reach it rather
than of every raise in the program. It is the same change that makes a
`None`-receiver check catchable ([ifa/165](../ifa/issues/165-none-reaching-an-operation-is-silently-accepted.md)
residual 1) and that [175](175-an-exception-raised-in-an-operator-method-does-not-propagate.md)
needs to prune checks before FA pays for them, so land it once, for all
three.

## Verification

The fixture flips to PASS on both backends.
