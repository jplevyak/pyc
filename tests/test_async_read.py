async def my_io():
    f = open("tests/async_simple.py", "r")
    return f.read(10)

async def main():
    print(1)
    val = await my_io()
    print(val)

main()
