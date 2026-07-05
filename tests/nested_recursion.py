# issues/001 follow-up: a nested def that recurses on itself must
# not be treated as capturing its own name (which would rebind the
# function Sym to a closure instance and trigger issue-007
# Finding 2 FA warnings). The recursive call resolves through the
# ordinary nesting_depth/display path -- always stack-disciplined.
def outer():
  def fact(n):
    if n <= 1:
      return 1
    return n * fact(n - 1)
  return fact(5)

print(outer())

# Mutual shape: recursion where the nested def is called twice from
# the enclosing function, each with independent arguments.
def outer2():
  def fib(n):
    if n < 2:
      return n
    return fib(n - 1) + fib(n - 2)
  return fib(10) + fib(5)

print(outer2())

# The mixed shape -- a nested recursive def that ALSO captures an
# enclosing local -- works since issues/007's split identity (the
# recursive self-reference resolves to the carrier instance, i.e.
# `self`, inside the body).
def make_counter(base):
  def count(n):
    if n <= 0:
      return base
    return 1 + count(n - 1)
  return count(3)

print(make_counter(10))
