# ifa/issues/090 repro 2 and its neighbours: a tuple compared against
# None, in both operand orders and through `in`.
#
# `move = None` then `while move not in moves:` is the shape. It used to
# print None -- a SILENT wrong answer -- because the generated
# tuple.__eq__ opens with len(t), which resolves to nothing when t is
# None, degrading the whole comparison so the loop body never ran.
def gen_moves():
    return [(1, 2), (3, 4)]

print((1, 2) == None)
print(None == (1, 2))
print((1, 2) != None)
print(None in gen_moves())
print((1, 2) in gen_moves())

move = None
while move not in gen_moves():
    move = (1, 2)
print(move)

t = ()
for i in range(3):
    t = t + (i, i + 1)
print(t)
