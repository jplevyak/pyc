async def my_io():
    f = open("async_simple.py", "r")
    return f.read(10)

async def main():
    print(1)
    val = await my_io()
    print(val)

res = __pyc_c_call__(int, "_CG_run_coro", int, main())
