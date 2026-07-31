import heapq
from heapq import heappush, heappop, heapify, heapreplace, heappushpop

def test_heappush_heappop():
    h = []
    heappush(h, 5)
    heappush(h, 1)
    heappush(h, 8)
    heappush(h, 3)
    heappush(h, 2)
    print("push_pop:")
    while len(h) > 0:
        print(heappop(h))

def test_heapify():
    data = [9, 4, 7, 1, 6, 2, 8, 3, 5, 0]
    heapify(data)
    print("heapify:")
    while len(data) > 0:
        print(heappop(data))

def test_heapreplace():
    h = [10, 20, 30, 40]
    heapify(h)
    old = heapreplace(h, 5)
    print("heapreplace_small:", old, h[0])
    old2 = heapreplace(h, 50)
    print("heapreplace_large:", old2, h[0])

def test_heappushpop():
    h = [10, 20, 30, 40]
    heapify(h)
    val1 = heappushpop(h, 5)
    print("heappushpop_small:", val1, h[0])
    val2 = heappushpop(h, 25)
    print("heappushpop_large:", val2, h[0])

def test_tuples():
    h = []
    heappush(h, (2, "medium"))
    heappush(h, (1, "high"))
    heappush(h, (3, "low"))
    print("tuples:")
    while len(h) > 0:
        item = heappop(h)
        print(item[0], item[1])

def test_dijkstra_style():
    # Representative of dijkstra2.py: bidirectional search fringe using
    # a list of heaps `fringe = [[], []]`, storing `(dist, node_id)` tuples
    # where dist is float and node_id is a coordinate tuple (x, y).
    fringe = [[], []]
    source = (0, 0)
    target = (4, 4)

    heappush(fringe[0], (0.0, source))
    heappush(fringe[1], (0.0, target))

    heappush(fringe[0], (1.5, (0, 1)))
    heappush(fringe[0], (2.0, (1, 0)))
    heappush(fringe[0], (1.5, (0, 2)))

    heappush(fringe[1], (1.0, (4, 3)))
    heappush(fringe[1], (2.5, (3, 4)))

    print("dijkstra_style forward pops:")
    while len(fringe[0]) > 0:
        dist, node = heappop(fringe[0])
        print(dist, node[0], node[1])

    print("dijkstra_style backward pops:")
    while len(fringe[1]) > 0:
        dist, node = heappop(fringe[1])
        print(dist, node[0], node[1])

def main():
    test_heappush_heappop()
    test_heapify()
    test_heapreplace()
    test_heappushpop()
    test_tuples()
    test_dijkstra_style()

main()
