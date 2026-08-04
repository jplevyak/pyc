def get_time() -> float:
    return __pyc_c_call__(float, "_CG_get_time")

async def sleep(seconds: float):
    __pyc_c_call__(int, "__pyc_sleep__", float, seconds)

async def main():
    start = get_time()
    await sleep(0.05)
    end = get_time()
    print(end - start >= 0.05)

res = __pyc_c_call__(int, "_CG_run_coro", int, main())
