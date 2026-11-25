# pyc shim for the standard `dataclasses` module (issues/195, issues/216).
#
# `@dataclass` and `field(...)` are consumed by the frontend
# (python_ifa_dataclass.cc), which generates the class CPython's decorator
# builds; a use it does not consume is a compile error there. The functions
# below reach methods that lowering generates on each dataclass, and only
# when the program uses them:
#
#   fields(x)    x.__pyc_dc_fields__(), a classmethod, so a class or an
#                instance; one tuple of Field per class, made with it.
#   astuple(x)   x.__pyc_dc_astuple__(), a tuple with a type per field.
#   asdict(x)    x.__pyc_dc_asdict__(), a dict literal of the fields: its
#                values must have one representable type.
#   replace(x, k=v, ...)   lowered at the call to a generated method named for
#                its set of keywords (__pyc_dc_replace_N__), which calls the
#                class with every other init field taken from x.
#
# Field has name, init, repr, compare, hash and kw_only; type, default,
# default_factory and metadata are not provided. make_dataclass, InitVar and
# KW_ONLY are not here, so a program that uses them is refused at import.


class FrozenInstanceError(AttributeError):
    pass


class Field:
    def __init__(self, name, init, repr, compare):
        self.name = name
        self.init = init
        self.repr = repr
        self.compare = compare
        self.hash = None
        self.kw_only = False


# The base the lowering adds to every dataclass when the program converts
# one (astuple, asdict), so the conversion can tell a nested dataclass
# instance from any other value by isinstance, which folds per contour.
class __pyc_dataclass__:
    pass


def dataclass(cls):
    return cls


def field():
    return None


def fields(class_or_instance):
    return class_or_instance.__pyc_dc_fields__()


def astuple(obj):
    return obj.__pyc_dc_astuple__()


def asdict(obj):
    return obj.__pyc_dc_asdict__()


def replace(obj):
    # Every call is lowered by the frontend; this binds the name.
    return obj
