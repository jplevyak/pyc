d = {x: x * 2 for x in [1, 2, 3]}
print(len(d))
print(d[1])
print(d[2])
print(d[3])

evens = {y: y for y in range(10) if y % 2 == 0}
print(len(evens))
print(evens[4])
print(evens[6])

squares = {n: n * n for n in [1, 2, 3, 4]}
print(len(squares))
print(squares[3])

total = 0
for k in squares:
    total += squares[k]
print(total)
