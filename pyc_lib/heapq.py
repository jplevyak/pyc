# pyc shim for the standard `heapq` module (binary min-heap on a plain
# list). Algorithm mirrors CPython's heapq.py (siftdown/siftup); list
# shrinking uses slice-assignment to an empty list since pyc's list
# has no pop().

def _siftdown(heap, startpos, pos):
    newitem = heap[pos]
    while pos > startpos:
        parentpos = (pos - 1) >> 1
        parent = heap[parentpos]
        if newitem < parent:
            heap[pos] = parent
            pos = parentpos
            continue
        break
    heap[pos] = newitem

def _siftup(heap, pos):
    endpos = len(heap)
    startpos = pos
    newitem = heap[pos]
    childpos = 2 * pos + 1
    while childpos < endpos:
        rightpos = childpos + 1
        if rightpos < endpos and not heap[childpos] < heap[rightpos]:
            childpos = rightpos
        heap[pos] = heap[childpos]
        pos = childpos
        childpos = 2 * pos + 1
    heap[pos] = newitem
    _siftdown(heap, startpos, pos)

def heappush(heap, item):
    heap.append(item)
    _siftdown(heap, 0, len(heap) - 1)

def heappop(heap):
    n = len(heap)
    lastelt = heap[n - 1]
    heap[n - 1:n] = []
    if heap:
        returnitem = heap[0]
        heap[0] = lastelt
        _siftup(heap, 0)
        return returnitem
    return lastelt

def heapreplace(heap, item):
    returnitem = heap[0]
    heap[0] = item
    _siftup(heap, 0)
    return returnitem

def heappushpop(heap, item):
    if heap and heap[0] < item:
        tmp = heap[0]
        heap[0] = item
        item = tmp
        _siftup(heap, 0)
    return item

def heapify(heap):
    n = len(heap)
    i = n // 2 - 1
    while i >= 0:
        _siftup(heap, i)
        i -= 1
