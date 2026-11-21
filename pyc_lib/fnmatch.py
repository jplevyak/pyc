# pyc shim for the standard `fnmatch` module: shell-style wildcard
# matching (*, ?, [seq], [!seq]) with a backtracking scan, no `re`.
#
# The pattern language is CPython's fnmatch.translate, exactly (issues/202):
# only `!` negates a set (`[^x]` is the set of `^` and `x`), a `]` first in
# a set is a member, a `-` first or last is a member, a reversed range such
# as `z-a` is dropped (CPython removes it from the regex), `[]` with no
# closing `]` and an unterminated `[` match a literal `[`, and fnmatch() is
# case-sensitive on POSIX, where os.path.normcase changes nothing. This
# shim used to negate on `^`, end a set at a leading `]`, and lowercase
# both sides in fnmatch().


# A set's members as CPython's translate builds its regex class from
# pattern[i:j] (the text between `[` and `]`): split at each `-` that can
# form a range, drop reversed ranges, and read `!` first as negation.
# Returns (negated, ranges) where ranges is a list of (lo, hi) chars, or
# None for a set that never matches.
def _parse_set(pattern, i, j):
    if '-' not in pattern[i:j]:
        chunks = [pattern[i:j]]
    else:
        chunks = []
        k = i + 2 if pattern[i] == '!' else i + 1
        while True:
            k = pattern.find('-', k, j)
            if k < 0:
                break
            chunks.append(pattern[i:k])
            i = k + 1
            k = k + 3
        chunk = pattern[i:j]
        if chunk:
            chunks.append(chunk)
        else:
            chunks[-1] = chunks[-1] + '-'
        # Remove empty ranges -- invalid in RE.
        k = len(chunks) - 1
        while k > 0:
            if chunks[k - 1][-1] > chunks[k][0]:
                chunks[k - 1] = chunks[k - 1][:-1] + chunks[k][1:]
                del chunks[k]
            k -= 1
    stuff = '-'.join(chunks)
    if not stuff:
        return None
    # The class as the regex reads it: each chunk's characters are
    # literals (translate escapes them), and the `-` between two chunks is
    # a range operator when a literal precedes it that is not already the
    # end of a range, and a literal follows; otherwise a literal `-`.
    toks = []
    t = 0
    while t < len(chunks):
        if t > 0:
            toks.append('')
        for ch in chunks[t]:
            toks.append(ch)
        t += 1
    neg = toks[0] == '!'
    q = 1 if neg else 0
    ranges = []
    while q < len(toks):
        lo = toks[q]
        if lo == '':
            lo = '-'
        if q + 2 < len(toks) and toks[q + 1] == '' and toks[q + 2] != '':
            ranges.append((lo, toks[q + 2]))
            q += 3
        else:
            ranges.append((lo, lo))
            q += 1
    return (neg, ranges)


def _set_matches(s, ch):
    neg, ranges = s
    hit = False
    for lo, hi in ranges:
        if lo <= ch and ch <= hi:
            hit = True
            break
    return hit != neg


# Where the set starting at pattern[pi] == '[' ends: the index of its
# closing `]`, or -1 when it is unterminated (the `[` is then a literal).
def _set_end(pattern, pi):
    plen = len(pattern)
    j = pi + 1
    if j < plen and pattern[j] == '!':
        j += 1
    if j < plen and pattern[j] == ']':
        j += 1
    while j < plen and pattern[j] != ']':
        j += 1
    if j >= plen:
        return -1
    return j


def _match(name, pattern):
    nlen = len(name)
    plen = len(pattern)
    ni = 0
    pi = 0
    star_pi = -1
    star_ni = -1
    while ni < nlen:
        step = 0
        if pi < plen and pattern[pi] == '*':
            star_pi = pi
            star_ni = ni
            pi += 1
            continue
        if pi < plen and pattern[pi] == '[':
            end = _set_end(pattern, pi)
            if end < 0:
                if name[ni] == '[':
                    step = 1
            else:
                s = _parse_set(pattern, pi + 1, end)
                if s is not None and _set_matches(s, name[ni]):
                    step = end + 1 - pi
        elif pi < plen and (pattern[pi] == '?' or pattern[pi] == name[ni]):
            step = 1
        if step > 0:
            ni += 1
            pi += step
        elif star_pi >= 0:
            star_ni += 1
            ni = star_ni
            pi = star_pi + 1
        else:
            return False
    while pi < plen and pattern[pi] == '*':
        pi += 1
    return pi == plen


def fnmatchcase(name, pattern):
    return _match(name, pattern)


def fnmatch(name, pattern):
    return _match(name, pattern)


def filter(names, pattern):
    r = []
    for n in names:
        if _match(n, pattern):
            r.append(n)
    return r
