# ifa/issues/055 (plcfrs, SECOND repro). Reduced from plcfrs.py by
# delta-debugging, 638 lines -> 36, AFTER PYC_CSSPLIT and PYC_ROUTECYCLE
# landed. The first repro
# (tests/dict_pair_swap_setdiff_nonconvergence.py) now passes; this is
# what is left, and it is a different shape -- the set difference that
# repro needed is NOT required here.
#
# FA hits the pass cap: final_pass=41 pass_limit_hit=1 CONVERGED=0,
# 152 violations, deterministic across 3 runs, 1.4 s.
#
# Seven ingredients, each verified necessary by ablation (removing any
# ONE of them converges):
#
#   1. the nested tuple unpack `for (rule, yf), weight in grammar`
#      (flattening it to `for rule, yf, weight` converges)
#   2. enumerate() over the nonterminal set
#   3. two dicts over the same pairs with SWAPPED key/value --
#      dict[str,int] and dict[int,str]
#   4. the `array("B", ...)` arity vector, indexed by a Rule field
#   5. Rule as a CLASS (a plain tuple in its place converges)
#   6. comparing the round trip, `yf == arraytoyf(args, lengths)`
#   7. the zip() generator inside arraytoyf
#
# Not required, though plcfrs has them: sorted(), the
# ["Epsilon","ROOT"] + ... concatenation, the set difference, the
# lexicon loop, Terminal, the four per-nonterminal rule lists, distinct
# array typecodes ("I" vs "H" -- both "I" still fails), and the
# conditional third Rule argument.
from array import array

class Rule:
    def __init__(self, lhs, rhs1, rhs2, args, lengths, prob):
        self.lhs = lhs
        self.rhs1 = rhs1
        self.rhs2 = rhs2
        self.args = args
        self.lengths = lengths

def yfarray(yf):
    initializer = [sum(1 << n for n, b in enumerate(a) if b) for a in yf]
    args = array("I", initializer)
    lengths = array("I", list(map(len, yf)))
    return args, lengths

def arraytoyf(args, lengths):
    return tuple(
        tuple(1 if a & (1 << m) else 0 for m in range(n)) for n, a in zip(lengths, args)
    )

def splitgrammar(grammar):
    nonterminals = list(enumerate(list(set(nt for (rule, yf), weight in grammar for nt in rule))))
    toid = dict((lhs, n) for n, lhs in nonterminals)
    tolabel = dict((n, lhs) for n, lhs in nonterminals)
    arity = array("B", [0] * len(nonterminals))
    for (rule, yf), w in grammar:
        args, lengths = yfarray(yf)
        assert yf == arraytoyf(args, lengths)
        r = Rule(toid[rule[0]], toid[rule[1]], toid[rule[2]],
                 args, lengths, 0.0)
        if arity[r.lhs] == 0:
            arity[r.lhs] = len(args)
    return toid

print(len(splitgrammar([((("S", "VP2", "VMFIN"), ((0, 1, 0),)), 1.0)])))
