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

# The standard exception hierarchy's commonly-referenced classes
# (issue 025's "has no type" bucket: an undefined name types as
# nothing and poisons everything downstream -- bh referenced
# SystemExit, tonyjpegdecoder/chaos ValueError, msp_ss KeyError/
# NotImplementedError, rubik RuntimeError, softrender/timsort
# IndexError, sudoku3 AssertionError, voronoi2 StopIteration, ...).
# Flat where CPython nests (LookupError/ArithmeticError subtleties
# don't matter without except-clause matching, issue 011); IOError
# is aliased to OSError as in Python 3.

class BaseException(object):
  args = ""
  def __init__(self, args=""):
    self.args = args
  def __str__(self):
    return self.args

class SystemExit(BaseException):
  pass

class KeyboardInterrupt(BaseException):
  pass

class StopIteration(Exception):
  pass

class ArithmeticError(Exception):
  pass

class ZeroDivisionError(ArithmeticError):
  pass

class OverflowError(ArithmeticError):
  pass

class AssertionError(Exception):
  pass

class AttributeError(Exception):
  pass

class LookupError(Exception):
  pass

class IndexError(LookupError):
  pass

class KeyError(LookupError):
  pass

class NameError(Exception):
  pass

class NotImplementedError(Exception):
  pass

class OSError(Exception):
  pass

class RuntimeError(Exception):
  pass

class TypeError(Exception):
  pass

class ValueError(Exception):
  pass

# Py3 aliases OSError; subclasses here (class-alias assignment is a
# separate, untested shape in the builtin module, and the difference
# is unobservable without except-clause matching).
class IOError(OSError):
  pass

class EnvironmentError(OSError):
  pass
