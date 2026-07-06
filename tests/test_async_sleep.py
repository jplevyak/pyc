def get_time() -> float:
    return __pyc_c_call__(float, "_CG_get_time")

async def sleep(seconds):
    return seconds

async def main():
    start = get_time()
    await sleep(1)
    end = get_time()
    print(end - start >= 0)

main()

