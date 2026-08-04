# issue 028: `raise Exception(...)` regressed by the qualified-static-
# dispatch commit (a32a6467, "Add @staticmethod/@classmethod and
# qualified static dispatch") -- shedskin's richards/bh examples both
# hit "warning: 'Exception' has no type" on exactly this shape
# (richards.py:235: raise Exception("Bad task id %d" % id)). Root
# cause: pyc's builtins never defined a real Exception class, so a
# bare `Exception(...)` construction (as opposed to a user subclass)
# resolved to a non-type Sym. Fixed by 6d3bf055 ("Frontend: builtin
# Exception class + break-label scoping fix", 2026-07-14) -- this test
# is the permanent regression guard the issue's own verification plan
# asked for.

def task_id(n):
    if n > 5:
        raise Exception("Bad task id %d" % n)
    return n

try:
    print(task_id(3))
    print(task_id(10))
except Exception as e:
    print("caught:", e)
