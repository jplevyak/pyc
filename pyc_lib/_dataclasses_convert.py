# The recursion of dataclasses.astuple / asdict (issues/216): CPython's
# _astuple_inner / _asdict_inner. Imported into a module by the dataclass
# lowering only when the program converts a dataclass, so a program that
# does not never gets tuple.__pyc_map__ (python_ifa_main.cc), whose
# per-arity overloads cost analysis time where they are not called.
import copy
from dataclasses import __pyc_dataclass__


def _astuple_value(v):
    if isinstance(v, __pyc_dataclass__):
        return v.__pyc_dc_astuple__()
    if isinstance(v, list):
        return [_astuple_value(x) for x in v]
    if isinstance(v, dict):
        return {_astuple_value(k): _astuple_value(x) for k, x in v.items()}
    if isinstance(v, tuple):
        return v.__pyc_map__(_astuple_value)
    return copy.deepcopy(v)


def _asdict_value(v):
    if isinstance(v, __pyc_dataclass__):
        return v.__pyc_dc_asdict__()
    if isinstance(v, list):
        return [_asdict_value(x) for x in v]
    if isinstance(v, dict):
        return {_asdict_value(k): _asdict_value(x) for k, x in v.items()}
    if isinstance(v, tuple):
        return v.__pyc_map__(_asdict_value)
    return copy.deepcopy(v)
