# pyc shim for the standard `dataclasses` module (issues/195).
#
# `@dataclass` and `field(...)` are consumed by the frontend
# (python_ifa_dataclass.cc), which generates the class CPython's decorator
# builds; a use it does not consume is a compile error there. These
# definitions only let `from dataclasses import dataclass, field` resolve.
# fields(), asdict(), astuple(), replace(), make_dataclass(), InitVar and
# KW_ONLY are not here, so a program that uses them is refused at import.


class FrozenInstanceError(AttributeError):
    pass


def dataclass(cls):
    return cls


def field():
    return None
