async def foo():
    await bar()
    
async def test_loops():
    async for i in my_iter():
        print(i)
        
async def test_with():
    async with my_context() as ctx:
        print(ctx)
