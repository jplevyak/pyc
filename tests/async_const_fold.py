# issues/022 follow-up: an `await` whose full call chain is
# compile-time-constant (a literal argument through to a provably
# constant return) used to have its coroutine's real body silently
# skipped -- FA correctly proves the RESULT value ahead of time, but
# the compiler was treating that as license to skip actually running
# the coroutine, dropping any of its own side effects (here, the
# print inside step()). Root cause: `ifa/optimize/inline.cc`'s
# sub_constants pass unconditionally substituted a bare constant Var
# for ANY live PNode's constant-valued rvals, including `await`'s own
# operand -- severing the link back to the real call that constructed
# the awaited coroutine. `step(1)`'s argument and return are both
# literal/provably-constant on purpose, specifically to keep
# exercising this fold path (unlike async_driven.py's file-derived
# inputs, which sidestep it entirely).

async def step(x):
    print("in step, x =", x)
    return x + 1

async def main():
    a = await step(1)
    print("a:", a)

res = __pyc_c_call__(int, "_CG_run_coro", int, main())
