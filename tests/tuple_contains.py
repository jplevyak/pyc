# issue 025 (collatz): membership test on a tuple literal
# (`rest9 in (2, 4, 5, 8)`). tuple was missing __contains__ (only list
# had it), so `x in (...)` aborted at runtime ("getter not resolved").
xs = (2, 4, 5, 8)
print(3 in xs)
print(4 in xs)
print(9 not in xs)
print(2 in (2,))          # single-element tuple
print("b" in ("a", "b"))  # str-element tuple
n = 5
if n in (1, 3, 5, 7):
    print("odd-ish")
c = 0
for k in range(10):
    if k in (2, 4, 6, 8):
        c += 1
print(c)
