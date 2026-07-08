class Node:
    def __init__(self, v):
        self.v = v

class G:
    def __init__(self):
        self.start = None
    def set(self, n):
        self.start = n

def main():
    g = G()
    if not g.start:
        print("empty")
    g.set(Node(5))
    if not g.start:
        print("still empty")
    else:
        print("has", g.start.v)
main()
