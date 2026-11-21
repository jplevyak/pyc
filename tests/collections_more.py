# collections: Counter, OrderedDict, and the rest of deque and defaultdict (issues/207).
from collections import Counter, OrderedDict, deque, defaultdict
c = Counter("abracadabra")
print(repr(c), c["a"], c["z"], len(c), c.most_common(2), c.most_common(), c.total(), sorted(c.elements()))
c.update("aaz")
c.subtract({"b": 5})
print(c, Counter(), Counter({"x": 2, "y": 1}), Counter(cats=4, dogs=2))
d = Counter("aab")
e = Counter("abbc")
print(d + e, d - e, d | e, d & e, +Counter({"a": -1, "b": 2}), -Counter({"a": -1, "b": 2}))
alias = d
d += e
print(alias, d == Counter("aaabbbc"), Counter(a=1) == Counter(a=1, b=0), Counter(a=1) != Counter(a=2))
del d["zz"]
del d["a"]
print(d, "b" in d, list(d), dict(d))
words = ["x", "y", "x"]
cnt = Counter(words)
print(cnt.most_common(1)[0], cnt.get("q", 9))

od = OrderedDict()
od["b"] = 1
od["a"] = 2
od["c"] = 3
print(od, list(od), list(reversed(od)), OrderedDict(), OrderedDict([(1, 2), (3, 4)]))
od.move_to_end("b")
print(od)
od.move_to_end("c", last=False)
print(od, od.popitem(), od.popitem(last=False), od)
o1 = OrderedDict([("x", 1), ("y", 2)])
o2 = OrderedDict([("y", 2), ("x", 1)])
print(o1 == o2, o1 == {"y": 2, "x": 1}, o1.copy() == o1)
try:
    od.move_to_end("nope")
except KeyError as err:
    print("KeyError", err)

q = deque([1, 2, 3])
q.append(4)
q.appendleft(0)
print(q, len(q), q[0], q[-1], 2 in q, q.count(2), q.index(3))
q.rotate(2)
print(q)
q.rotate(-1)
print(q)
q.extendleft([10, 11])
print(q, q.popleft(), q.pop(), q)
q.remove(10)
q.reverse()
print(q, list(reversed(q)))
q.insert(1, 99)
q[0] = 7
del q[-1]
print(q, q.copy(), q == deque([7, 99, 4, 3, 2, 1]) if False else q == q.copy())
b = deque(maxlen=3)
for i in range(6):
    b.append(i)
print(b, b.maxlen, deque([1]).maxlen)
b.appendleft(9)
print(b)
b.extend([7, 8])
print(b, deque())
qq = deque([1, 2])
qq += [3]
print(qq)
try:
    deque().pop()
except IndexError as err:
    print("IndexError", err)
try:
    deque([1]).remove(5)
except ValueError as err:
    print("ValueError", err)

dd = defaultdict(list)
dd["a"].append(1)
dd["b"].append(2)
dd["a"].append(3)
print(dict(dd), dd.setdefault("c", [5]), sorted(dd.keys()))
