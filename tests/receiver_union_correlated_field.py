# ifa/174: one method shared by several subclasses reads a field whose type
# is correlated with the receiver's class, and passes it to a per-class
# override. richards' `Task.runTask` does `self.fn(msg, self.handle)`; with
# one runTask contour for all four task classes, `self.handle` was the
# union of every record, so HandlerTask.fn called `h.workInAdd` on a
# DeviceTaskRec and the program failed to type. The dispatch failure is
# now backtracked to the load that made the union (`self.handle` over a
# multi-class `self`) and runTask is split by the class of `self`.
class Rec(object):
    pass
class ARec(Rec):
    def __init__(self):
        self.a = 1
    def bump(self):
        self.a += 1
        return self.a
class BRec(Rec):
    def __init__(self):
        self.b = "x"
    def grow(self):
        self.b += "y"
        return self.b
class Task(object):
    def __init__(self, r):
        self.handle = r
    def run(self):
        return self.fn(self.handle)
class ATask(Task):
    def fn(self, r):
        h = r
        assert isinstance(h, ARec)
        return str(h.bump())
class BTask(Task):
    def fn(self, r):
        h = r
        assert isinstance(h, BRec)
        return h.grow()
tasks = [ATask(ARec()), BTask(BRec()), ATask(ARec())]
out = []
for i in range(3):
    for t in tasks:
        out.append(t.run())
print(" ".join(out))
