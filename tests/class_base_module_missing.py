# issues/191: a base named through a module that has no such name. CPython
# raises AttributeError; pyc refuses at compile time, naming the module.
import from_import_helper


class Bad(from_import_helper.Nope):
    pass


print(Bad())
