async def get_value(x):
    return x * 2

async def main():
    val = await get_value(21)
    print("Value:", val)

main()
