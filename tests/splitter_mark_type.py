# Two list comprehensions over lists of different element types. Once {A,B}
# forms at list.append's value formal, every call edge carries {A,B}, so the
# splitter must find the separation through the CreationSets. `CALLS` pins
# that `self.aas[-1].ay()` resolves directly; `STAGES` pins which stages
# get there.
class A:
    def ay(self):
        return 1

class B:
    def bee(self):
        return 2

class Holder:
    def __init__(self):
        self.aas = []
        self.bbs = []

    def fill(self):
        self.aas.append(A())
        self.bbs.append(B())

    def prune(self):
        self.bbs = [x for x in self.bbs]
        self.aas = [y for y in self.aas]

    def use(self):
        return self.aas[-1].ay()

h = Holder()
h.fill()
h.prune()
print(h.use())
