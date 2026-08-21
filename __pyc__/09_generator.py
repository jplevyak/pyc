# Generator objects (issues/014). A generator function is compiled as
# a C++20 coroutine (pyc_c_runtime.h's _CG_Generator); the coroutine
# handle is smuggled through a plain int field exactly like
# __pyc_file__ smuggles a FILE* (07_file.py). __iter__ returns self
# (a generator is its own iterator, matching real Python) so
# PY_for_stmt's existing generic __iter__/__pyc_more__/__next__
# lowering works unmodified.
#
# `primed` tracks whether the coroutine has already been advanced to
# a value that hasn't been consumed by __next__/send yet. Both
# __pyc_more__ and __next__ check it before deciding whether to
# advance, so two call patterns both work correctly:
#   - for-loop driving: __pyc_more__ (loop condition) advances and
#     primes, __next__ (loop body) consumes without re-advancing --
#     exactly one advance per element, as before.
#   - bare, repeated __next__()/.send() calls with no __pyc_more__ in
#     between (the "interleaved manual next()" case, issue 014 item
#     3): each call finds `primed` False, so each one does its own
#     advance-then-consume -- no missed or double advances.
# Nothing resumes the coroutine until the first __pyc_more__/__next__/
# send call, matching real Python's laziness (creating a generator
# runs none of its body).
#
# .send(v) delivers v as the value of the generator's currently
# paused `x = yield foo` expression, then resumes and returns the
# newly yielded value -- like __next__, but for a non-None delivery.
# Mixing .send() into an in-progress __pyc_more__/__next__ for-loop
# alternation on the same generator is not addressed (not a pattern
# real Python code combines either); .send()-driven and for-loop-
# driven use are independent, not simultaneous, usage modes.
#
# StopIteration(value) on exhaustion (issues/014, landed 2026-08-04):
# __next__/send raise it -- matching real Python's __next__ contract
# -- once advancing reports no more values. __pyc_more__ (the for-loop
# peek used by PY_for_stmt) stays a plain boolean and never raises:
# for-loops never call __next__ past what __pyc_more__ already
# confirmed, so this doesn't affect them, only bare/repeated manual
# __next__()/.send() calls run past exhaustion. `value` is int64 only
# (0 for a bare/fall-through generator exit, matching pyc's existing
# "smuggle through int64" compromise for yielded/sent values -- real
# Python reports None there instead of 0).
#
# issues/014 (yield from): `has_next == False` after advancing means
# EITHER the generator body ran to completion OR it raised some OTHER
# exception internally -- both reach the coroutine's co_return/done
# state identically (see cg.cc/cg_emit_llvm.cc's is_generator
# P_prim_reply handling: a raise inside the body sets __pyc_exc__,
# a global, then unwinds to the SAME exit label normal completion
# uses). __pyc_exc__ is an ordinary builtin-module global
# (08_exception.py), directly readable here: checking it distinguishes
# the two cases. When set, this is NOT exhaustion -- return without
# raising StopIteration (masking the real exception would be wrong)
# and let it flow out normally; the CALLER's own post-call check
# (emit_exc_check, python_ifa_build_if1.cc -- inserted automatically
# after any user-code call once pyc_program_has_raise is armed)
# propagates it from there, the same mechanism every other raise
# already relies on. Without this check, __next__/.send() would
# incorrectly raise StopIteration instead, masking the real exception
# -- confirmed via a direct repro (`raise` inside a generator body,
# no yield from involved, caught by an outer try/except of the SAME
# exception class: without this fix the wrong exception type reached
# the handler and propagated unhandled instead).

class __pyc_generator__:
  handle = 0
  primed = False
  has_next = False
  # issues/114: NO initializer for nextval. `nextval = 0` pinned the
  # value channel to int, so a generator yielding tuples handed back a
  # reinterpreted POINTER and `for x in gen(): print(x)` printed an
  # address. Its type comes from the c_calls below instead.
  def __init__(self, handle):
    # `handle`'s runtime value is the coroutine handle, but its TYPE is
    # now `{None, <what this generator yields>}` -- each `yield` moves
    # its value into the generator function's fn->ret, so the wrapper's
    # `handle_result` carries the yielded types out. FA already clones
    # __pyc_generator__ per creation site, which is pyc's equivalent of
    # shedskin's one-class-per-generator.
    self.handle = handle
  def __iter__(self):
    return self
  def __pyc_advance__(self):
    self.has_next = __pyc_c_call__(bool, "_CG_generator_advance", int, self.handle)
    if self.has_next:
      # Result type from self.handle, not a hardcoded int -- the
      # runtime channel is a machine word either way.
      self.nextval = __pyc_c_call__(self.handle, "_CG_generator_value", int, self.handle)
  def __pyc_more__(self):
    if not self.primed:
      self.__pyc_advance__()
      self.primed = True
    return self.has_next
  def __next__(self):
    if not self.primed:
      self.__pyc_advance__()
    self.primed = False
    if not self.has_next:
      if __pyc_exc__ is not None:
        # issues/114: NOT `return 0`. Never observed -- an exception is
        # pending -- but an int literal here unions int64 into the
        # channel's type by itself.
        return self.nextval
      raise StopIteration(__pyc_c_call__(int, "_CG_generator_return_value", int, self.handle))
    return self.nextval
  def __contains__(self, item):
    # ifa/issues/090 / issues/025 item 4: `x in gen()`.
    # python_ifa_build_if1.cc lowers `in` unconditionally to a direct
    # __contains__ dispatch on the right operand, with no fallback to
    # the general iterable protocol when the method is absent -- so a
    # generator, which has __iter__/__pyc_more__/__next__ but never a
    # __contains__, could not be tested for membership at all. Even
    # `3 in gen()` over plain ints failed ("illegal call argument
    # type"). Exactly the gap __dict_iter__.__contains__ was added for
    # (07_dict.py).
    #
    # Consumes the generator, which is what CPython's `in` does too.
    while self.__pyc_more__():
      if self.__next__() == item:
        return True
    return False
  def send(self, value):
    self.has_next = __pyc_c_call__(bool, "_CG_generator_send", int, self.handle, int, value)
    self.primed = False
    if not self.has_next:
      if __pyc_exc__ is not None:
        # issues/114: NOT `return 0`. Never observed -- an exception is
        # pending -- but an int literal here unions int64 into the
        # channel's type by itself.
        return self.nextval
      raise StopIteration(__pyc_c_call__(int, "_CG_generator_return_value", int, self.handle))
    self.nextval = __pyc_c_call__(self.handle, "_CG_generator_value", int, self.handle)
    return self.nextval
