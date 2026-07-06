async def get_value():
    return 42

async def main():
    val = await get_value()
    print("Value:", val)

main()
