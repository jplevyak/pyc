def test_narrowing(flag):
    if flag:
        x = 5
    else:
        x = "hi"
    
    # x is split here, but never passed to a primitive.
    # The Liveness-aware BOXING check will see x is only used by phy/phi
    # and successfully suppress the BOXING violation.
    if flag:
        y = 1
    else:
        y = 2
    return y

print(test_narrowing(True))
print(test_narrowing(False))
