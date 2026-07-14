# break/continue after a nested loop must bind to the OUTER loop.
# A save/restore bug in PY_for_stmt/PY_while_stmt label handling once
# left the INNER loop's break label in the scope after its subtree,
# so this `break` lowered to a goto placed straight after its own
# target label -- an infinite loop at runtime and a no-path-to-exit
# CFG region that crashed the dominator build (othello2, issue 025).
i = 0
while i < 4:
    for j in range(2):
        print(i * 10 + j)
    if i == 2:
        break
    i += 1
print("after while")

# Same shape with the loops swapped: continue after an inner while.
for k in range(4):
    m = 0
    while m < 1:
        print(100 + k)
        m += 1
    if k >= 1:
        continue
    print(200 + k)
print("done")
