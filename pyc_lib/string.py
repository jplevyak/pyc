# Python 3 canonical names (the module was carried over with the
# Python 2 `letters`/`lowercase`/`uppercase` names; keep those as
# aliases below so nothing that used them breaks).
ascii_lowercase = "abcdefghijklmnopqrstuvwxyz"
ascii_uppercase = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
ascii_letters = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"
letters = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"
lowercase = "abcdefghijklmnopqrstuvwxyz"
uppercase = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
digits = "0123456789"
hexdigits = "0123456789abcdefABCDEF"
octdigits = "01234567"
punctuation = "!\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~"
whitespace = " \t\n\r\x0b\x0c"
printable = digits + letters + punctuation + whitespace

# issues/041: capwords/split/join were no-op stubs (always "" / []
# regardless of input). split/join are legacy Python-2-style module
# functions that don't actually exist in real Python 3's `string`
# module at all (superseded by str.split()/sep.join()) -- kept here,
# implemented for real, in the same Python-2-compat spirit as the
# letters/lowercase/uppercase aliases above. capwords *is* real in
# Python 3 and is used directly by at least one corpus example.

def capwords(s, sep=None):
    words = s.split(sep)
    result = []
    for w in words:
        if len(w) == 0:
            result.append(w)
        else:
            # No str.capitalize() in pyc's builtin str -- build it
            # from upper()/lower() on single-char slices instead.
            result.append(w[0].upper() + w[1:].lower())
    if sep is None:
        use_sep = " "
    else:
        use_sep = sep
    return use_sep.join(result)

def split(s, sep=None, maxsplit=-1):
    # maxsplit isn't supported by pyc's builtin str.split(); ignored.
    return s.split(sep)

def join(words, sep=" "):
    return sep.join(words)


# string.Template, as CPython's (issues/207): `$$` is a `$`, `$name` and
# `${name}` are placeholders whose name is `[_a-zA-Z][_a-zA-Z0-9]*`, and
# anything else after a `$` is an invalid placeholder. A value is
# converted with str(). Keyword values of different types in one call are
# refused (issues/189); a dict of them works when its values share a type.
def _tpl_is_start(c):
    return c == '_' or ('a' <= c and c <= 'z') or ('A' <= c and c <= 'Z')


def _tpl_is_part(c):
    return _tpl_is_start(c) or ('0' <= c and c <= '9')


class Template:
    delimiter = '$'

    def __init__(self, template):
        self.template = template

    # The pattern's matches, in order: (kind, start, end, name) with kind
    # 0 escaped, 1 named, 2 braced, 3 invalid; `start` is the `$`.
    def _scan(self):
        out = []
        s = self.template
        n = len(s)
        i = 0
        while i < n:
            if s[i] != '$':
                i += 1
                continue
            j = i + 1
            if j < n and s[j] == '$':
                out.append((0, i, j + 1, ''))
                i = j + 1
                continue
            if j < n and _tpl_is_start(s[j]):
                k = j + 1
                while k < n and _tpl_is_part(s[k]):
                    k += 1
                out.append((1, i, k, s[j:k]))
                i = k
                continue
            if j < n and s[j] == '{' and j + 1 < n and _tpl_is_start(s[j + 1]):
                k = j + 2
                while k < n and _tpl_is_part(s[k]):
                    k += 1
                if k < n and s[k] == '}':
                    out.append((2, i, k + 1, s[j + 1:k]))
                    i = k + 1
                    continue
            out.append((3, i, j, ''))
            i = j
        return out

    def _invalid(self, i):
        lines = self.template[:i].splitlines(True)
        if not lines:
            colno = 1
            lineno = 1
        else:
            colno = i - len(''.join(lines[:-1]))
            lineno = len(lines)
        raise ValueError('Invalid placeholder in string: line %d, col %d' % (lineno, colno))

    def substitute(self, mapping=None, **kws):
        out = []
        last = 0
        for m in self._scan():
            kind = m[0]
            out.append(self.template[last:m[1]])
            last = m[2]
            if kind == 0:
                out.append('$')
            elif kind == 3:
                self._invalid(m[1] + 1)
            else:
                name = m[3]
                if name in kws:
                    out.append(str(kws[name]))
                elif mapping is not None and name in mapping:
                    out.append(str(mapping[name]))
                else:
                    raise KeyError(repr(name))
        out.append(self.template[last:])
        return ''.join(out)

    def safe_substitute(self, mapping=None, **kws):
        out = []
        last = 0
        for m in self._scan():
            kind = m[0]
            out.append(self.template[last:m[1]])
            last = m[2]
            if kind == 0:
                out.append('$')
            elif kind == 3:
                out.append('$')
            else:
                name = m[3]
                if name in kws:
                    out.append(str(kws[name]))
                elif mapping is not None and name in mapping:
                    out.append(str(mapping[name]))
                else:
                    out.append(self.template[m[1]:m[2]])
        out.append(self.template[last:])
        return ''.join(out)

    def get_identifiers(self):
        ids = []
        for m in self._scan():
            if (m[0] == 1 or m[0] == 2) and m[3] not in ids:
                ids.append(m[3])
        return ids

    def is_valid(self):
        for m in self._scan():
            if m[0] == 3:
                return False
        return True
