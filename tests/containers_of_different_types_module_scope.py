# ifa/172: several containers of one kind with different element types,
# at MODULE scope. Each pair used to stay on one CreationSet (the setter
# walk stopped at the folded global loads), so the int set and the str set
# failed to compile, and the int set next to the float set was coerced to
# float and answered membership wrongly on the LLVM backend.
t = set()
for i in range(2000):
    t.add(i * 7)
n = 0
for i in range(2000):
    if i in t:
        n += 1
print(len(t), n)
w = set(["a", "b", "a"])
print(len(w), "a" in w, "z" in w)
f = set([1.5, 2.5, 1.5])
print(len(f), 2.5 in f, f == set([2.5, 1.5]))
tt = set([(1, 2), (3, 4)])
print(len(tt), (3, 4) in tt)
