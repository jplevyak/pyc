# Two modules each defining a global of the same name. The LLVM backend
# reused an existing LLVM global with the same NAME, so both `VALUE`s were
# one slot (minilight's camera/scene/triangle `SEARCH` patterns: camera's
# 3-group pattern parsed with scene's 2 groups).
import same_name_global_helper_a as a
import same_name_global_helper_b as b

print(a.get(), b.get(), a.VALUE, b.VALUE)
