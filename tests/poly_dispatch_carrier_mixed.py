# ifa/issues/030(a): a call site whose candidate set mixes a plain
# top-level function and a closure-carrier instance (the wrapper
# returned by a decorator) used to crash: the closure-carrier
# candidate has no named method-pointer slot for
# poly_dispatch_classtag_targets to find, so it fell into the
# plain-function value-identity dispatch route, which compares the
# runtime value against the function's own code address -- a
# carrier instance (a heap pointer) can never equal that, so every
# branch missed and the shared "no branch matched" runtime assert
# fired. Fixed by recognizing a candidate whose own receiver type is
# the synthesized closure-carrier record and dispatching it by
# classtag compare (calling it directly -- no stored slot needed,
# since it's the statically-known implementation for that tag)
# instead of routing it through the plain-function route.
#
# C backend only: this fix is scoped to cg.cc. The LLVM backend
# (cg_emit_llvm.cc) doesn't support mixed classtag+plain-function
# dispatch chains at all yet -- an independent, already-documented
# gap (ifa/issues/030-DISPATCH's "Status check" section) -- so it
# still produces wrong output here (reads garbage instead of
# crashing). Hence the .check_fail sidecar: remove it once LLVM
# grows the same mixed-dispatch chain cg.cc has.
def double(f):
    def wrapper(x):
        return f(x) * 2
    return wrapper

def add_one(x):
    return x + 1

def make_dispatcher(flag):
    if flag:
        return double(add_one)
    else:
        return add_one

def main():
    for flag in (True, False):
        g = make_dispatcher(flag)
        print(g(5))

main()
