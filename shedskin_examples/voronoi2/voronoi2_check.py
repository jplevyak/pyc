# pyc check driver (added, not an upstream file: see ../PYC_CHANGES.md).
# voronoi2's __main__ sets c.doPrint = 0, so all it prints is TIME. This
# reads the same sites and runs the same diagram with printing on: every
# site, vertex, bisector and edge.
# corpus_sweep.sh runs this in place of voronoi2.py in run/check mode.
import voronoi2

pts = []
fp = open('voronoi2.txt', 'r')
for line in fp:
    fld = line.split()
    pts.append(voronoi2.Site(float(fld[0]), float(fld[1])))
fp.close()
c = voronoi2.Context()
c.doPrint = 1
voronoi2.voronoi(voronoi2.SiteList(pts), c)
