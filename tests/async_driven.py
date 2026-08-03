# issue 022: async/await syntax and both backends' coroutine machinery
# were already implemented, but NO existing test actually drove the
# event loop to completion -- every prior test just constructed a
# coroutine object and let it sit (real Python: "RuntimeWarning:
# coroutine was never awaited"), so nothing had ever verified real
# execution. Driving one (via _CG_run_coro, the same call
# tests/test_async_net.py already used) surfaced that the LLVM
# backend's `await` never actually resumed the awaited coroutine or
# rescheduled the awaiter once it finished -- a function with two or
# more sequential/nested real awaits silently stopped executing after
# the first suspend, no error, just missing output. Fixed by giving
# is_async functions a side struct (mirroring is_generator's
# gen_state) that tracks an `awaiter` link, and chaining through the
# existing event loop (_CG_event_loop_spawn) instead of hand-rolling
# LLVM's symmetric-transfer pattern. The C backend's C++20 coroutines
# handle this chaining automatically via the language's own awaiter
# protocol -- but had its own bug, also fixed here: `co_await`'s
# result (a void*) was never cast to the awaiting variable's real
# type, breaking any function with 2+ sequential awaits (an
# already-documented gap in a code comment, never hit by any test
# until this file).
#
# Inputs are derived from reading this repo's own async_simple.py
# (mirroring test_async_read.py's existing file-read pattern) rather
# than being literal constants, so FA can't constant-fold the whole
# await chain away at compile time -- a provably-constant chain
# bypasses the real coroutine machinery entirely (confirmed while
# debugging: a literal-argument await silently skipped executing the
# awaited function's body but still produced a plausible-looking
# result) and would give a false pass here.

def read_seed():
    f = open("async_simple.py", "r")
    line = f.readline()
    return ord(line[0])  # 'a' from "async" == 97, deterministic

def read_str_seed():
    f = open("async_simple.py", "r")
    return f.read(2)  # "as", deterministic

async def step(x):
    return x + 1

async def sequential():
    n = read_seed() - 92  # 5
    a = await step(n)
    b = await step(a)
    c = await step(b)
    print(a, b, c)

async def level3(x):
    return x + 100

async def level2(x):
    r = await level3(x)
    return r + 10

async def level1(x):
    r = await level2(x)
    return r + 1

async def nested():
    n = read_seed() - 94  # 3
    result = await level1(n)
    print(result)

# An awaited coroutine returning a non-int value (a string) -- exercises
# the type conversion an int-only payload wouldn't catch.
async def echo(s):
    return s + "!"

async def strings():
    n = read_str_seed()
    result = await echo(n)
    print(result)

res1 = __pyc_c_call__(int, "_CG_run_coro", int, sequential())
res2 = __pyc_c_call__(int, "_CG_run_coro", int, nested())
res3 = __pyc_c_call__(int, "_CG_run_coro", int, strings())
