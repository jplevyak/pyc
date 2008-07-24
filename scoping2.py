x = 2

def f():
    x = 3
    
    def g():
        global x
        print x
        x = 4
    
    print x
    g()

f()
print x

