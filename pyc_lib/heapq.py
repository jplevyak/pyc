# pyc shim for the standard `heapq` module (binary min-heap on a plain
# list). Algorithm mirrors CPython's heapq.py (siftdown/siftup),
# including its `heap.pop()` for the shrink.
#
# It used to shrink with `heap[n-1:n] = []`, because `list.pop()` did not
# exist when this shim was written; issues/025 R1 added it and the note
# went stale. That workaround was not merely redundant -- it was a
# CORRECTNESS bug under ifa/128's start-merged posture (PYC_CSDCPA1).
# `__pyc_setslice__` opens with `merge_in(self, v)`, which asserts that
# v's elements flow into self; with one CreationSet per sym the `[]`
# literal here shares a contour with every other empty list in the
# program, so any element any caller put in one leaked into every heap
# that had an element popped. On tests/test_heapq.py that put `tuple`
# into the element of `data = [9, 4, 7, ...]`, and codegen then emitted
# `((_CG_void*)(_CG_list_ptr(t10)))[6] = 8` -- an int stored through a
# pointer-typed element. See ifa/issues/133, which fixed the same shape
# in `__pyc__/04_sequence.py`'s `__delitem__` and predicted this one:
# a hand-written slice assignment keeps the merge, because there the
# `merge_in` is real.

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
    lastelt = heap.pop()
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
