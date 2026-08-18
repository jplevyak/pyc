# issues/107: this is a SYNTAX test -- it is compiled, never run. It used
# to reference bar/my_iter/my_context without defining them, which pyc
# silently accepted (minting never-assigned globals). Now that an
# undefined name is a hard error, the stubs are declared; the syntax
# under test is unchanged.
async def bar():
    return 1

async def my_iter():
    return [1]

async def my_context():
    return 1

async def foo():
    await bar()
    
async def test_loops():
    async for i in my_iter():
        print(i)
        
async def test_with():
    async with my_context() as ctx:
        print(ctx)
