# ifa/issues/049's repro: the only call of `risky` raises, so its return
# value was bottom and pyc refused the program ("expression has no type").
# CPython raises ValueError; pyc reports it as an uncaught exception.
def risky(n):
    if n > 5:
        raise ValueError("too big")
    return n


print("before")
print(risky(9))
