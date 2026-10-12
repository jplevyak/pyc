# pyc check driver (added, not an upstream file: see ../PYC_CHANGES.md).
# dijkstra's __main__ prints its paths only when n == 4999, in a loop of
# 500, so all it prints is TIME. This runs the same seeded searches and
# prints every path, its length, and the sub-path check.
# corpus_sweep.sh runs this in place of dijkstra.py in run/check mode.
import random
import dijkstra

for n in range(40):
    random.seed(n)
    G = dijkstra.Graph()
    dijkstra.G = G  # Graph.distance reads the module global G, as __main__ sets it
    s = G.V[random.randint(0, len(G.V) - 1)]
    while True:
        t = G.V[random.randint(0, len(G.V) - 1)]
        if t is not s:
            break
    S = dijkstra.dijkstra(G, t, s)
    if S:
        print("dijkstra %s ---> %s: " % (s, t), S, G.distance(s, S))
        alarms = 0
        for inter in S[:-1]:
            S1 = dijkstra.dijkstra(G, t, inter)
            if S1 != S[(len(S) - len(S1)):]:
                alarms += 1
        print("  sub-paths", len(S) - 1, "alarms", alarms)
    else:
        print("dijkstra %s ---> %s: no path" % (s, t))
