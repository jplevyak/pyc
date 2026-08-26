# ifa/issues/055 (plcfrs), minimal repro. Reduced from plcfrs.py's
# splitgrammar(), 638 lines -> 9, by delta-debugging; the 500-line
# program is not needed and neither is the parser.
#
# Four ingredients, each verified necessary by ablation (removing any
# one converges):
#
#   1. a TUPLE UNPACK in the generator feeding the set
#      (`for rule, weight in grammar`; `for rule in grammar` converges)
#   2. the SET DIFFERENCE
#   3. enumerate()
#   4. two dicts built from the SAME pairs with SWAPPED key/value --
#      dict[str,int] and dict[int,str]. Making both the same
#      orientation converges, so it is the swap, not the second dict.
#
# Not needed: sorted(), and the `["a","b"] + ...` list concatenation
# that plcfrs has around the sorted() call.
#
# FA hits the pass cap: final_pass=51 pass_limit_hit=1 CONVERGED=0,
# 58 violations, deterministic across runs.
def f(grammar):
    nts = list(set(nt for rule, weight in grammar for nt in rule) - set(["a", "b"]))
    pairs = list(enumerate(nts))
    toid = dict((lhs, n) for n, lhs in pairs)
    tolabel = dict((n, lhs) for n, lhs in pairs)
    return toid

rules = [(("S", "VP2"), 1.0)]
print(len(f(rules)))
