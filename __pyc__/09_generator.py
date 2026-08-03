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

class __pyc_generator__:
  handle = 0
  primed = False
  has_next = False
  nextval = 0
  def __init__(self, handle):
    self.handle = handle
  def __iter__(self):
    return self
  def __pyc_advance__(self):
    self.has_next = __pyc_c_call__(bool, "_CG_generator_advance", int, self.handle)
    if self.has_next:
      self.nextval = __pyc_c_call__(int, "_CG_generator_value", int, self.handle)
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
      raise StopIteration(__pyc_c_call__(int, "_CG_generator_return_value", int, self.handle))
    return self.nextval
  def send(self, value):
    self.has_next = __pyc_c_call__(bool, "_CG_generator_send", int, self.handle, int, value)
    self.primed = False
    if not self.has_next:
      raise StopIteration(__pyc_c_call__(int, "_CG_generator_return_value", int, self.handle))
    self.nextval = __pyc_c_call__(int, "_CG_generator_value", int, self.handle)
    return self.nextval
