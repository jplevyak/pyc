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

Exception VALUE flow along the call graph: a handler reads the union of
the raises that can reach it, not of every raise in the program. That
means the pending exception travels like a return value: a per-contour
"exception out" channel in FA, flowing from a callee to the handlers of
its callers, in place of the one global slot. The frontend's 21 uses of
`__pyc_exc__` (`python_ifa_build_if1.cc`) and the generator protocol
(`__pyc__/09_generator.py`) move with it. It is a new channel through
FA, so it must also say whether it creates splitting demands.

**Not the same change as [175](175-an-exception-raised-in-an-operator-method-does-not-propagate.md)
step 2** (corrected 2026-11-14; this section used to say "land it once,
for all three"). 175 and
[ifa/165](../ifa/issues/165-none-reaching-an-operation-is-silently-accepted.md)
residual 1 need exception LIVENESS: a check whose exceptional arm FA
does not analyse until a resolved callee can raise. That leaves the
slot's type the whole program's union, so this refusal stays. Value flow
would give liveness too, but liveness is the cheaper half and is where
the silent wrong answers are (179, 175), so it comes first; this waits
until value flow is wanted for its own sake, such as precise
`except X as e` typing in general.

Priority: low. A compile-time refusal of an uncommon idiom (`e.value`
read in an explicit handler, in a program whose generators return two
value types), already a `.known_issue`.

## Verification

The fixture flips to PASS on both backends.
