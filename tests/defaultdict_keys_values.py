# issues/025 "has no type" bucket, defaultdict half: pyc_lib/collections.py's
# defaultdict wraps its own internal dict (self.d) but had no
# .keys()/.values()/.items()/__len__/__iter__ of its own either --
# shedskin's mastermind2.py (`for i in b.values()`, b a defaultdict)
# hit this independently of dict's own gap (07_dict.py).
from collections import defaultdict

b = defaultdict(int)
b["a"] = 1
b["b"] = 2
print(sorted(b.keys()))
print(sorted(b.values()))
print(len(b))
