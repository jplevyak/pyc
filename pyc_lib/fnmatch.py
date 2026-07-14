# pyc shim for the standard `fnmatch` module: shell-style wildcard
# matching (*, ?, [seq], [!seq]) via a classic backtracking scan --
# no dependency on `re`.

def _match(name, pattern):
    nlen = len(name)
    plen = len(pattern)
    ni = 0
    pi = 0
    star_pi = -1
    star_ni = -1
    while ni < nlen:
        if pi < plen and pattern[pi] == '*':
            star_pi = pi
            star_ni = ni
            pi += 1
        elif pi < plen and pattern[pi] == '[':
            end = pi + 1
            neg = False
            if end < plen and (pattern[end] == '!' or pattern[end] == '^'):
                neg = True
                end += 1
            start_set = end
            while end < plen and pattern[end] != ']':
                end += 1
            if end >= plen:
                # unterminated set: '[' matches itself
                if pattern[pi] == name[ni]:
                    ni += 1
                    pi += 1
                elif star_pi >= 0:
                    star_ni += 1
                    ni = star_ni
                    pi = star_pi + 1
                else:
                    return False
            else:
                matched = False
                j = start_set
                while j < end:
                    if j + 2 < end and pattern[j + 1] == '-':
                        if pattern[j] <= name[ni] and name[ni] <= pattern[j + 2]:
                            matched = True
                        j += 3
                    else:
                        if pattern[j] == name[ni]:
                            matched = True
                        j += 1
                if neg:
                    matched = not matched
                if matched:
                    ni += 1
                    pi = end + 1
                elif star_pi >= 0:
                    star_ni += 1
                    ni = star_ni
                    pi = star_pi + 1
                else:
                    return False
        elif pi < plen and (pattern[pi] == '?' or pattern[pi] == name[ni]):
            ni += 1
            pi += 1
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
    return _match(name.lower(), pattern.lower())

def filter(names, pattern):
    r = []
    for n in names:
        if fnmatch(n, pattern):
            r.append(n)
    return r
