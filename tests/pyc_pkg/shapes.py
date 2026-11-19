# For tests/class_base_module_attr.py: a base named through a module that a
# relative `from . import helper` bound (shedskin's
# `class ConstVisitor(ast_utils.BaseNodeVisitor)`).
from . import helper


class Square(helper.Shape):
    def __init__(self, s):
        helper.Shape.__init__(self, "square")
        self.s = s

    def area(self):
        return self.s * self.s
