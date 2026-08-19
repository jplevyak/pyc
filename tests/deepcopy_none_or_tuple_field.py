# issues/110: deepcopy of a record whose field is {None, tuple}.
#
# Seven lines. Under PYC_MAKESEQ=1 PYC_TUPLE_AS_LIST=1 this emits a
# void-typed copy destination and the C will not compile:
#
#   _CG_void t3;
#   t1 = (_CG_void)_CG_prim_copy_dst(_CG_void, t2);
#   error: invalid application of 'sizeof' to an incomplete type 'void'
#
# `args` is None on the leaf and a tuple on the root, so deepcopy's
# generic copy sees a {None, tuple} union and resolves the destination to
# nothing. It is the issues/018 union shape reached through copy, not a
# defect in make_seq -- tuple(...) on its own matches CPython on len,
# indexing, repr and hash.
#
# PASSES at the default settings (both flags off), so it is not tagged:
# it is here to pin the shape that blocks defaulting them.
import copy
class T:
    def __init__(self, args=None):
        self.args = args
tree = T(tuple([T(None)]))
c = copy.deepcopy(tree)
print(c.args[0].args)
