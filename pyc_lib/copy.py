# pyc shim for the standard `copy` module

def copy(obj):
    return __pyc_primitive__(__pyc_symbol__("copy"), obj)

def deepcopy(obj):
    # issues/029: one dispatch -- __deepcopy__ is defined everywhere.
    # Record classes get a SYNTHESIZED per-class recursive method
    # (gen_class_pyda: shallow clone + per-field __deepcopy__), lists
    # a handwritten element-recursive one (__pyc__/04_sequence.py),
    # None identity, and everything else (scalars, strings, tuples)
    # the __pyc_any_type__ shallow fallback. Recursion through nested
    # types rides normal method dispatch, so each level gets a
    # monomorphic contour via recursive-ES splitting (issues/025 R1
    # item 5). v1 has NO memo table: unlike CPython, shared subtrees
    # are duplicated and CYCLIC structures do not terminate -- the
    # corpus need (genetic2's genome trees) is trees.
    return obj.__deepcopy__()
