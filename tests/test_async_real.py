def get_arg():
    return 21

async def get_value(x: int):
    for i in range(1):
        x = x + 0
    return x * 2

async def main():
    val = await get_value(get_arg())
    print("Value:", val)

res = __pyc_c_call__(int, "_CG_run_coro", int, main())
