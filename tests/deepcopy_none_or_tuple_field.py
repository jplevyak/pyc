# issues/110: deepcopy of a record whose field is {None, tuple}.
#
# This FAILED under PYC_MAKESEQ=1 PYC_TUPLE_AS_LIST=1 with a void-typed
# copy destination, which looked like a copy-path defect and was not:
#
#   t1 = (_CG_void)_CG_prim_copy_dst(_CG_void, t2);
#   error: invalid application of 'sizeof' to an incomplete type
#
# The make_seq constraint read the source container's element types with
# update_gen -- a SNAPSHOT with no ordering guarantee. On the LAST pass
# the source read bottom, so the tuple CreationSet stayed tuple_able and
# clone.cc gave it RECORD layout with zero members. Every downstream
# symptom (this void copy destination, and "bad getter" on a plain
# subscript) was that one empty record. Fixed in 50ebc8aa by flowing the
# element through durable edges via vector_elems.
#
# Kept untagged because it passes at the default settings too: it pins
# the shape, and it is the shape that regressed the hardest.
import copy
class T:
    def __init__(self, args=None):
        self.args = args
tree = T(tuple([T(None)]))
c = copy.deepcopy(tree)
print(c.args[0].args)
