# global defined ahead of use
def y():
    global z 
    z = 2
y()
print z
