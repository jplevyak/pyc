VAL = 7

def hval():
    return VAL


class Shape:
    def __init__(self, name):
        self.name = name

    def area(self):
        return 0

    def describe(self):
        return self.name + " " + str(self.area())
