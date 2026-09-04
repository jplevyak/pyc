# pyc shim for the standard `copy` module

def copy(obj):
    # ifa/issues/118: one dispatch, mirroring deepcopy below. The bare
    # copy PRIMITIVE is identity for any non-Type_RECORD destination
    # (cg.cc), so it aliased a generic list -- chess's legalMoves does
    # `board2 = copy(board)` and then mutates board2, which corrupted
    # the caller's board and made every search return a cutoff.
    return obj.__pyc_copy__()

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
