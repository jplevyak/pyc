# pyc shim for `__future__` (issues/195). Every feature a Python 3 program
# names here is already pyc's behaviour, and `annotations` (PEP 563) is too:
# pyc never evaluates an annotation. The names only have to exist.
nested_scopes = 1
generators = 1
division = 1
absolute_import = 1
with_statement = 1
print_function = 1
unicode_literals = 1
generator_stop = 1
annotations = 1
