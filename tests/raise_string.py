# Python-2-style `raise "message"` (a bare string as an exception).
# In Python 3 this is a TypeError, but the shedskin corpus (chess) uses
# it as a fatal-error idiom. pyc wraps a raised string/bytes literal in
# Exception(...) so the raised object is a real exception (catchable,
# str()-able) instead of leaking a raw str into the __pyc_exc__ slot --
# which, combined with other unions, tipped FA into a NOTYPE cascade.

def check(n):
    if n < 0:
        raise "negative not allowed"
    return n * 2

# caught as a normal Exception; message preserved via str(e)
def guarded(n):
    try:
        return check(n)
    except Exception as e:
        return -1

print(check(4))       # 8
print(guarded(4))     # 8
print(guarded(-1))    # -1  (the string-raise was caught)

# bare except also catches it
def bare(n):
    try:
        return check(n)
    except:
        return 99

print(bare(-5))       # 99
print(bare(3))        # 6

# a raise-string path that never executes must not poison unrelated
# code (the shape that broke chess: a live global computed nearby)
data = [i for i in range(10) if i % 2 == 0]

def helper(x):
    if x == 999999:
        raise "unreachable"
    return x

print(helper(len(data)))   # 5
print(data)                # [0, 2, 4, 6, 8]
