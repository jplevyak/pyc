def f(x):
    return {'x': x, 'y': x * 2}
r = f(4)
print(r['x'], r['y'])

d2 = {'x': 10}
print(d2.get('x', 0))
print(d2.get('z', 99))
d2.update({'y': 20})
print(d2['y'])
for k in d2:
    print(k)
