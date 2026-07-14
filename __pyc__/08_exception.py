# Exception base class (ifa/issues/042). pyc has no exception
# *control flow* yet (issue 011: raise/try-except don't transfer
# control), but exception *classes* are ordinary classes: programs
# subclass Exception, construct instances, and format them with
# str() -- and without a real builtin Exception, `class X(Exception)`
# resolved the base to a plain non-type Sym whose null meta_type
# segfaulted build_type_hierarchy (amaze, tictactoe, voronoi2, rdb,
# msp_ss in the shedskin corpus). Explicitly derives from object
# (builtin-module classes are exempt from the implicit object base,
# see build_syms_pyda's PY_classdef case) so user subclasses still
# reach object-level defaults (__pyc_to_bool__, __not__, ...) through
# the chain.
#
# `args` is a plain string, not real Python's tuple: every corpus use
# is `Exception("message")` or a subclass with its own __init__, and
# a string field keeps str(e) trivial without tuple formatting.

class Exception(object):
  args = ""
  def __init__(self, args=""):
    self.args = args
  def __str__(self):
    return self.args
