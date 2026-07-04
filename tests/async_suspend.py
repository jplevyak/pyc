def get_arg():
    return 21

async def get_value(x):
    return x * 2

async def main():
    arg = get_arg()
    val = await get_value(arg)
    print("Value:", val)

main()
