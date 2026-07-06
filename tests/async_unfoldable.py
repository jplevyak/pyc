async def get_value(x):
    return x * 2

async def main(arg):
    val = await get_value(arg)
    print("Value:", val)

main(2)
