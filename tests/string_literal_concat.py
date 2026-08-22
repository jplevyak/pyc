# issues/117: two decoder bugs in eval_string_pyda, both silent.
#
# 1. Adjacent string literals concatenate in Python ('ab' 'cd' == 'abcd')
#    and the grammar already accepted it (python.g's atom: STRING+), but
#    only the FIRST fragment was decoded and the rest were dropped.
#    sunfish writes its 120-character board that way, so `initial` came
#    out 10 characters long and every board scan found nothing.
# 2. The end-of-literal scan stopped at the first quote CHARACTER rather
#    than the first unescaped one, so any literal containing an escaped
#    quote was truncated: len('a\'b') was 1.

a = ('ab' 'cd' 'ef')
print(len(a), a)

b = (
    'ab'  # a comment between fragments is part of the raw span
    'cd'  # and so is the newline
    'ef'
)
print(len(b), b)

# Line continuation between fragments.
c = 'ab' \
    'cd'
print(len(c), c)

print(len('a\'b'), 'a\'b')
print(len("x\"y"), "x\"y")
print(len(r'a\'b'), r'a\'b')
print(len('''tri''' 'ple'), '''tri''' 'ple')
print(len("emb\nedded" "next"))

# bytes fragments concatenate too.
print(b'ab' b'cd')

# An f-string may be any fragment, not just the first -- the dispatch
# has to look at all of them.
n = 5
print(f'n={n}' ' done')
print('start ' f'{n}')
print(f'a{n}' f'b{n}')

# Escapes still decode normally after all this.
print(len('a\tb\\c'), 'a\tb\\c' == 'a\tb\\c')
