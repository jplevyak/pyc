def add_one(x): return x + 1
def replacement(x): return x + 100
def get_it(f): return replacement
add_one = get_it(add_one)
print(add_one(5))
