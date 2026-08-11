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
