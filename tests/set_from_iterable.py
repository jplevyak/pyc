# issue 025 "has no type" bucket: set(iterable) had no dispatch at
# all (set.__init__ only accepts zero args) -- 1-arg calls silently
# dropped their argument, producing a bottom-typed result that
# cascaded into invalid generated C for real-world programs using it
# (e.g. shedskin's rubik2.py: state_ids = set([current_id])).
s = set([1, 2, 3])
print(len(s))
print(1 in s)
print(4 in s)

dup = set([1, 2, 2, 3, 1])
print(len(dup))

# from a tuple, and from a range -- any iterable, not just a list.
t = set((4, 5, 6))
print(len(t))
r = set(range(5))
print(len(r))

empty = set([])
print(len(empty))

# update() -- also newly added, same construction path.
u = set([1, 2])
u.update([2, 3, 4])
print(len(u))
print(3 in u)
