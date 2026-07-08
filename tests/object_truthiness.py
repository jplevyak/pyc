# Truthiness and `not` on plain instances (issue 025):
# - bare `class A:` implicitly derives from object (Python 3), so
#   object-level defaults (__pyc_to_bool__, __not__) dispatch
# - object.__pyc_to_bool__ previously negated __bool__, inverting
#   truthiness for every object-derived class
# - `not x` now lowers through period dispatch, so inherited __not__
#   resolves for user objects and None
class Bare:
    def __init__(self):
        self.x = 1

class WithBase(object):
    def __init__(self):
        self.x = 2

class Node:
    def __init__(self, v):
        self.v = v

def main():
    a = Bare()
    if a:
        print("bare truthy")
    if not a:
        print("bare not-truthy (wrong)")
    b = WithBase()
    if b:
        print("based truthy")
    if not b:
        print("based not-truthy (wrong)")
    s = None
    if not s:
        print("none is falsy")
    s = Node(5)
    if not s:
        print("node falsy (wrong)")
    else:
        print("node truthy", s.v)
main()
