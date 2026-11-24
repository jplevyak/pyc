# pyc's `re`: a backtracking regular-expression engine written in Python
# (issues/210). A pattern is parsed to a tree, the tree is emitted as a flat
# instruction list, and one recursive matcher walks the list, backtracking
# at each SPLIT: the design of Russ Cox's backtrack.c, with Python's
# leftmost-first (not longest) semantics.
#
# Syntax: literals and escapes (\n \t \xhh \uhhhh \0oo ...), '.', classes
# [...] with ranges and \d \w \s \D \W \S, groups (capturing, (?:...),
# (?P<name>...)), alternation, * + ? {m,n} {m,} {,n} {m} and their lazy
# forms, ^ $ \A \Z \b \B, backreferences \1 and (?P=name), lookahead
# (?=...) (?!...), fixed-width lookbehind (?<=...) (?<!...), comments
# (?#...), inline flags (?imsx) at the start and scoped (?i:...) (?-i:...).
# Flags IGNORECASE, MULTILINE, DOTALL, VERBOSE, ASCII.
#
# API: compile, search, match, fullmatch, findall, finditer, sub, subn,
# split, escape, purge, error; Pattern and Match with CPython's methods and
# attributes. The matching rules for empty matches in findall, finditer,
# sub and split are CPython 3.7's: an empty match is not allowed at the
# position where the previous match ended empty.
#
# What differs from CPython, each refused rather than answered wrongly
# where it can be seen:
#   - findall() with two or more groups returns tuples, and with fewer
#     returns strings: one function whose element type depends on a count
#     known only when the pattern is parsed, at run time. pyc refuses it at
#     run time (NotImplementedError) and names finditer, whose
#     m.groups() is the same tuple. Typing it needs the group count as a
#     static fact (issues/210).
#   - Case folding and \w \d \s are ASCII. pyc's str indexes UTF-8 bytes
#     (issues/203), so '.' and a class match one byte of a non-ASCII
#     character.
#   - Possessive quantifiers, atomic groups (?>...), conditionals
#     (?(1)...) and variable-width lookbehind raise re.error naming them.
#   - Match.group() takes one argument; m.group(1, 2) is m.groups().
#   - A quantified group does not reset captures made inside it by an
#     iteration that later failed: such a capture can survive (rare).
#
# ifa/040 history: `.kids` is never rebound, only appended to. A wholesale
# rebind of a list field once corrupted an unrelated traversal; 040 is
# closed, and the style stays.

# --- flags: CPython's values -------------------------------------------------

NOFLAG = 0
TEMPLATE = 1
T = 1
IGNORECASE = 2
I = 2
LOCALE = 4
L = 4
MULTILINE = 8
M = 8
DOTALL = 16
S = 16
UNICODE = 32
U = 32
VERBOSE = 64
X = 64
DEBUG = 128
ASCII = 256
A = 256

_FLAG_NAMES = ["re.TEMPLATE", "re.IGNORECASE", "re.LOCALE", "re.MULTILINE",
               "re.DOTALL", "re.UNICODE", "re.VERBOSE", "re.DEBUG", "re.ASCII"]

_MAXSIZE = 9223372036854775807


class error(Exception):
    def __init__(self, msg, pattern="", pos=-1):
        self.msg = msg
        self.pattern = pattern
        self.pos = pos
        if pos >= 0:
            self._text = msg + " at position " + str(pos)
        else:
            self._text = msg
        Exception.__init__(self, self._text)

    def __str__(self):
        return self._text


# --- instructions ------------------------------------------------------------

OP_CHAR = 0
OP_ANY = 1
OP_CLASS = 2
OP_SPLIT = 3
OP_JMP = 4
OP_SAVE = 5
OP_MATCH = 6
OP_BOL = 7
OP_EOL = 8
OP_WORDB = 9
OP_BOS = 10
OP_EOS = 11
OP_LOOPINIT = 12
OP_LOOP = 13
OP_BACKREF = 14
OP_LOOK = 15
OP_LOOKEND = 16

N_LIT = 0
N_ANY = 1
N_CLASS = 2
N_CONCAT = 3
N_ALT = 4
N_GROUP = 5
N_STAR = 6
N_PLUS = 7
N_OPT = 8
N_REPEAT = 9
N_BOL = 10
N_EOL = 11
N_WORDB = 12
N_BOS = 13
N_EOS = 14
N_BACKREF = 15
N_LOOK = 16

_MAXCHAR = 1114111


def _is_word_char(c):
    o = ord(c)
    if o >= 48 and o <= 57:
        return True
    if o >= 65 and o <= 90:
        return True
    if o >= 97 and o <= 122:
        return True
    return o == 95


def _fold(o):
    # ASCII case folding of one ordinal, to lower case.
    if o >= 65 and o <= 90:
        return o + 32
    return o


def _swapcase(o):
    if o >= 65 and o <= 90:
        return o + 32
    if o >= 97 and o <= 122:
        return o - 32
    return o


class _ReOp:
    def __init__(self, kind):
        self.kind = kind
        self.ch = ''
        self.a = 0
        self.b = 0
        self.slot = 0
        self.neg = False
        self.icase = False
        self.lo = []
        self.hi = []


def _class_match(op, c):
    o = ord(c)
    n = len(op.lo)
    i = 0
    while i < n:
        if o >= op.lo[i] and o <= op.hi[i]:
            return not op.neg
        i += 1
    if op.icase:
        o2 = _swapcase(o)
        if o2 != o:
            i = 0
            while i < n:
                if o2 >= op.lo[i] and o2 <= op.hi[i]:
                    return not op.neg
                i += 1
    return op.neg


def _add_range(op, lo, hi):
    op.lo.append(lo)
    op.hi.append(hi)


def _digit_ranges(lo, hi):
    lo.append(48)
    hi.append(57)


def _word_ranges(lo, hi):
    lo.append(48)
    hi.append(57)
    lo.append(65)
    hi.append(90)
    lo.append(95)
    hi.append(95)
    lo.append(97)
    hi.append(122)


def _space_ranges(lo, hi):
    # str.isspace() over ASCII: \t \n \v \f \r, \x1c-\x1f and ' '.
    lo.append(9)
    hi.append(13)
    lo.append(28)
    hi.append(32)


def _add_escape_ranges(op, e, negated):
    # \d \w \s (or, negated, their complements) into a class's ranges. The
    # ranges are sorted and disjoint, so the complement is the gaps.
    lo = []
    hi = []
    if e == 'd':
        _digit_ranges(lo, hi)
    elif e == 'w':
        _word_ranges(lo, hi)
    else:
        _space_ranges(lo, hi)
    if not negated:
        i = 0
        while i < len(lo):
            _add_range(op, lo[i], hi[i])
            i += 1
        return
    nxt = 0
    i = 0
    while i < len(lo):
        if lo[i] > nxt:
            _add_range(op, nxt, lo[i] - 1)
        nxt = hi[i] + 1
        i += 1
    if nxt <= _MAXCHAR:
        _add_range(op, nxt, _MAXCHAR)


class _ReNode:
    def __init__(self, kind, flags):
        self.kind = kind
        self.flags = flags
        self.ch = ''
        self.cls = _ReOp(OP_CLASS)
        self.kids = []
        self.group_index = -1
        self.rmin = 0
        self.rmax = -1
        self.greedy = True
        self.neg = False
        self.behind = False


def _hexval(c):
    o = ord(c)
    if o >= 48 and o <= 57:
        return o - 48
    if o >= 97 and o <= 102:
        return o - 87
    if o >= 65 and o <= 70:
        return o - 55
    return -1


def _is_octal(c):
    return c >= '0' and c <= '7'


def _is_digit(c):
    return c >= '0' and c <= '9'


def _is_ascii_letter(c):
    return (c >= 'a' and c <= 'z') or (c >= 'A' and c <= 'Z')


def _flag_of(c):
    if c == 'i':
        return IGNORECASE
    if c == 'm':
        return MULTILINE
    if c == 's':
        return DOTALL
    if c == 'x':
        return VERBOSE
    if c == 'a':
        return ASCII
    if c == 'u':
        return UNICODE
    if c == 'L':
        return LOCALE
    return 0


class _ReParser:
    def __init__(self, pattern, flags):
        self.pat = pattern
        self.pos = 0
        self.n = len(pattern)
        self.ngroups = 0
        self.flags = flags
        self.names = {}
        self.open_groups = []
        self.seen_atom = False
        self._esc = ''

    def fail(self, msg, pos):
        raise error(msg, self.pat, pos)

    def peek(self):
        if self.pos < self.n:
            return self.pat[self.pos]
        return ''

    def peek_at(self, k):
        if self.pos + k < self.n:
            return self.pat[self.pos + k]
        return ''

    def advance(self):
        c = self.pat[self.pos]
        self.pos += 1
        return c

    def node(self, kind):
        return _ReNode(kind, self.flags)

    def skip_verbose(self):
        # VERBOSE: whitespace and '#' comments between tokens are ignored.
        if (self.flags & VERBOSE) == 0:
            return
        while self.pos < self.n:
            c = self.pat[self.pos]
            if c == ' ' or c == '\t' or c == '\n' or c == '\r' or c == '\f' or c == '\v':
                self.pos += 1
            elif c == '#':
                while self.pos < self.n and self.pat[self.pos] != '\n':
                    self.pos += 1
            else:
                return

    def parse(self):
        node = self.parse_alt()
        if self.pos < self.n:
            # parse_alt stops only at ')' when no group is open.
            self.fail("unbalanced parenthesis", self.pos)
        return node

    def parse_alt(self):
        first = self.parse_concat()
        if self.peek() != '|':
            return first
        alt = self.node(N_ALT)
        alt.kids.append(first)
        while self.peek() == '|':
            self.pos += 1
            alt.kids.append(self.parse_concat())
        return alt

    def parse_concat(self):
        node = self.node(N_CONCAT)
        while True:
            self.skip_verbose()
            if self.pos >= self.n or self.peek() == '|' or self.peek() == ')':
                break
            r = self.parse_repeat()
            if r is not None:
                node.kids.append(r)
        return node

    def parse_int(self):
        s = ''
        while self.pos < self.n and _is_digit(self.pat[self.pos]):
            s = s + self.pat[self.pos]
            self.pos += 1
        if s == '':
            return -1
        return int(s)

    def quantifier(self, atom, kind, at):
        node = self.node(kind)
        node.kids.append(atom)
        if self.peek() == '?':
            self.pos += 1
            node.greedy = False
        elif self.peek() == '+':
            self.fail("possessive quantifiers are not supported by pyc", self.pos)
        return node

    def parse_repeat(self):
        start = self.pos
        atom = self.parse_atom()
        if atom is None:
            return None
        while True:
            self.skip_verbose()
            c = self.peek()
            at = self.pos
            if c == '*':
                self.pos += 1
                atom = self.quantifier(atom, N_STAR, at)
            elif c == '+':
                self.pos += 1
                atom = self.quantifier(atom, N_PLUS, at)
            elif c == '?':
                self.pos += 1
                atom = self.quantifier(atom, N_OPT, at)
            elif c == '{':
                save = self.pos
                self.pos += 1
                m = self.parse_int()
                mx = m
                ok = True
                if self.peek() == ',':
                    self.pos += 1
                    mx = self.parse_int()
                elif m < 0:
                    ok = False
                if ok and self.peek() == '}':
                    self.pos += 1
                    if m < 0:
                        m = 0
                    if mx >= 0 and mx < m:
                        self.fail("min repeat greater than max repeat", save + 1)
                    node = self.quantifier(atom, N_REPEAT, at)
                    node.rmin = m
                    node.rmax = mx
                    atom = node
                else:
                    # Not a quantifier: '{' is a literal, as CPython reads it.
                    self.pos = save
                    return atom
            else:
                return atom
            # A second quantifier on a quantified atom ("a**") is an error.
            c2 = self.peek()
            if c2 == '*' or c2 == '+' or (c2 == '{' and _is_digit(self.peek_at(1))):
                self.fail("multiple repeat", self.pos)
            if c2 == '?':
                self.fail("multiple repeat", self.pos)

    def parse_group_name(self, terminator):
        start = self.pos
        name = ''
        while self.pos < self.n and self.pat[self.pos] != terminator:
            name = name + self.pat[self.pos]
            self.pos += 1
        if self.pos >= self.n:
            self.fail("missing " + terminator + ", unterminated name", start)
        self.pos += 1
        if name == '':
            self.fail("missing group name", start)
        if not name.isidentifier():
            self.fail("bad character in group name '" + name + "'", start)
        return name

    def parse_atom(self):
        start = self.pos
        c = self.advance()
        if c == '*' or c == '+' or c == '?':
            self.fail("nothing to repeat", start)
        if c == '{' and _is_digit(self.peek()):
            # "{3}" with nothing before it: CPython reads it as a repeat.
            save = self.pos
            self.parse_int()
            if self.peek() == '}' or self.peek() == ',':
                self.fail("nothing to repeat", start)
            self.pos = save
        if c != '(':
            self.seen_atom = True
        if c == '.':
            return self.node(N_ANY)
        elif c == '^':
            return self.node(N_BOL)
        elif c == '$':
            return self.node(N_EOL)
        elif c == '(':
            return self.parse_paren(start)
        elif c == '[':
            return self.parse_class(start)
        elif c == '\\':
            return self.parse_escape(start)
        elif c == ')':
            self.fail("unbalanced parenthesis", start)
        node = self.node(N_LIT)
        node.ch = c
        return node

    def parse_sub(self, start):
        # The body of a group, through its ')'.
        sub = self.parse_alt()
        if self.peek() != ')':
            self.fail("missing ), unterminated subpattern", start)
        self.pos += 1
        return sub

    def parse_paren(self, start):
        if self.peek() != '?':
            self.seen_atom = True
            self.ngroups += 1
            gi = self.ngroups
            self.open_groups.append(gi)
            sub = self.parse_sub(start)
            self.open_groups.pop()
            node = self.node(N_GROUP)
            node.kids.append(sub)
            node.group_index = gi
            return node
        self.pos += 1
        c = self.peek()
        if c == ':':
            self.pos += 1
            self.seen_atom = True
            node = self.node(N_GROUP)
            node.kids.append(self.parse_sub(start))
            return node
        if c == 'P':
            self.pos += 1
            c2 = self.peek()
            if c2 == '<':
                self.pos += 1
                name = self.parse_group_name('>')
                if name in self.names:
                    self.fail("redefinition of group name '" + name + "' as group " + str(self.ngroups + 1) + "; was group " + str(self.names[name]), start + 4)
                self.seen_atom = True
                self.ngroups += 1
                gi = self.ngroups
                self.names[name] = gi
                self.open_groups.append(gi)
                sub = self.parse_sub(start)
                self.open_groups.pop()
                node = self.node(N_GROUP)
                node.kids.append(sub)
                node.group_index = gi
                return node
            if c2 == '=':
                self.pos += 1
                name = self.parse_group_name(')')
                if name not in self.names:
                    self.fail("unknown group name '" + name + "'", start + 4)
                return self.backref(self.names[name], start)
            self.fail("unknown extension ?P" + c2, start + 1)
        if c == '#':
            while self.pos < self.n and self.pat[self.pos] != ')':
                self.pos += 1
            if self.pos >= self.n:
                self.fail("missing ), unterminated comment", start)
            self.pos += 1
            return None
        if c == '=' or c == '!':
            self.pos += 1
            self.seen_atom = True
            node = self.node(N_LOOK)
            node.neg = c == '!'
            node.kids.append(self.parse_sub(start))
            return node
        if c == '<':
            c2 = self.peek_at(1)
            if c2 == '=' or c2 == '!':
                self.pos += 2
                self.seen_atom = True
                node = self.node(N_LOOK)
                node.neg = c2 == '!'
                node.behind = True
                body = self.parse_sub(start)
                w = _width(body)
                if w < 0:
                    self.fail("look-behind requires fixed-width pattern", -1)
                node.rmin = w
                node.kids.append(body)
                return node
            self.fail("unknown extension ?<" + c2, start + 1)
        if c == '>':
            self.fail("atomic groups (?>...) are not supported by pyc", start)
        if c == '(':
            self.fail("conditional groups (?(...)...) are not supported by pyc", start)
        # Inline flags: (?imsx) for the whole pattern, (?imsx-imsx:...) for
        # a group.
        on = 0
        off = 0
        minus = False
        while self.pos < self.n:
            f = self.peek()
            if f == '-':
                if minus:
                    self.fail("missing flag", self.pos)
                minus = True
                self.pos += 1
                continue
            v = _flag_of(f)
            if v == 0:
                break
            if minus:
                off = off | v
            else:
                on = on | v
            self.pos += 1
        f = self.peek()
        if f == ')':
            if minus:
                self.fail("missing :", self.pos)
            if self.seen_atom or start != 0 and not self._only_flags_before(start):
                self.fail("global flags not at the start of the expression", start)
            self.pos += 1
            self.flags = self.flags | on
            return None
        if f == ':':
            self.pos += 1
            saved = self.flags
            self.flags = (self.flags | on) & ~off
            self.seen_atom = True
            node = self.node(N_GROUP)
            node.kids.append(self.parse_sub(start))
            self.flags = saved
            return node
        if on == 0 and off == 0 and not minus:
            self.fail("unknown extension ?" + f, start + 1)
        self.fail("missing -, : or )", self.pos)

    def _only_flags_before(self, start):
        # Global flag groups may follow each other at the start: "(?i)(?m)".
        i = 0
        while i < start:
            if self.pat[i] != '(' or i + 1 >= start or self.pat[i + 1] != '?':
                return False
            j = i + 2
            while j < start and _flag_of(self.pat[j]) != 0:
                j += 1
            if j >= start or self.pat[j] != ')':
                return False
            i = j + 1
        return True

    def backref(self, gi, start):
        if gi in self.open_groups:
            self.fail("cannot refer to an open group", start)
        node = self.node(N_BACKREF)
        node.group_index = gi
        return node

    def class_char(self, start):
        # One character of a class, after any escape; -1 if the escape was
        # a class (\d ...), whose ranges are then in self._esc_class.
        c = self.advance()
        if c != '\\':
            return ord(c)
        if self.pos >= self.n:
            self.fail("bad escape (end of pattern)", self.pos - 1)
        e = self.advance()
        if e == 'd' or e == 'w' or e == 's' or e == 'D' or e == 'W' or e == 'S':
            self._esc = e
            return -1
        if e == 'b':
            return 8
        return self.char_escape(e, self.pos - 2, True)

    def char_escape(self, e, at, in_class):
        # The ordinal of a character escape; raises error for an unknown one.
        if e == 'n':
            return 10
        if e == 't':
            return 9
        if e == 'r':
            return 13
        if e == 'f':
            return 12
        if e == 'v':
            return 11
        if e == 'a':
            return 7
        if e == 'x':
            return self.hex_escape(2, at)
        if e == 'u':
            return self.hex_escape(4, at)
        if e == 'U':
            return self.hex_escape(8, at)
        if e == '0' or (in_class and _is_octal(e)):
            v = ord(e) - 48
            k = 0
            while k < 2 and self.pos < self.n and _is_octal(self.pat[self.pos]):
                v = v * 8 + ord(self.pat[self.pos]) - 48
                self.pos += 1
                k += 1
            if v > 255:
                self.fail("octal escape value \\" + self.pat[at + 1:self.pos] + " outside of range 0-0o377", at)
            return v
        if in_class and _is_digit(e):
            self.fail("bad escape \\" + e, at)
        if _is_ascii_letter(e):
            self.fail("bad escape \\" + e, at)
        return ord(e)

    def hex_escape(self, digits, at):
        v = 0
        k = 0
        while k < digits:
            if self.pos >= self.n or _hexval(self.pat[self.pos]) < 0:
                self.fail("incomplete escape \\" + self.pat[at + 1:self.pos], at)
            v = v * 16 + _hexval(self.pat[self.pos])
            self.pos += 1
            k += 1
        if v > _MAXCHAR:
            self.fail("bad escape \\" + self.pat[at + 1:self.pos], at)
        return v

    def parse_class(self, start):
        op = _ReOp(OP_CLASS)
        if self.peek() == '^':
            self.pos += 1
            op.neg = True
        first = True
        while True:
            if self.pos >= self.n:
                self.fail("unterminated character set", start)
            if self.peek() == ']' and not first:
                self.pos += 1
                break
            first = False
            self._esc = ''
            lo = self.class_char(start)
            if lo < 0:
                e = self._esc
                _add_escape_ranges(op, e.lower(), e != e.lower())
                if self.peek() == '-' and self.peek_at(1) != ']' and self.peek_at(1) != '':
                    self.fail("bad character range \\" + e + "-" + self.peek_at(1), self.pos - 2)
                continue
            hi = lo
            if self.peek() == '-' and self.peek_at(1) != ']' and self.peek_at(1) != '':
                rng = self.pos - 1
                self.pos += 1
                self._esc = ''
                hi = self.class_char(start)
                if hi < 0:
                    self.fail("bad character range", rng)
                if hi < lo:
                    self.fail("bad character range " + chr(lo) + "-" + chr(hi), rng)
            _add_range(op, lo, hi)
        node = self.node(N_CLASS)
        op.icase = (self.flags & IGNORECASE) != 0
        node.cls = op
        return node

    def parse_escape(self, start):
        if self.pos >= self.n:
            self.fail("bad escape (end of pattern)", start)
        e = self.advance()
        if e == 'd' or e == 'w' or e == 's' or e == 'D' or e == 'W' or e == 'S':
            op = _ReOp(OP_CLASS)
            _add_escape_ranges(op, e.lower(), False)
            op.neg = e != e.lower()
            op.icase = False
            node = self.node(N_CLASS)
            node.cls = op
            return node
        if e == 'b':
            return self.node(N_WORDB)
        if e == 'B':
            node = self.node(N_WORDB)
            node.neg = True
            return node
        if e == 'A':
            return self.node(N_BOS)
        if e == 'Z':
            return self.node(N_EOS)
        if e >= '1' and e <= '9':
            # A backreference of up to two digits, or an octal escape of
            # three.
            if self.pos + 1 < self.n and _is_octal(e) and _is_octal(self.pat[self.pos]) and _is_octal(self.pat[self.pos + 1]):
                v = (ord(e) - 48) * 64 + (ord(self.pat[self.pos]) - 48) * 8 + ord(self.pat[self.pos + 1]) - 48
                self.pos += 2
                if v > 255:
                    self.fail("octal escape value \\" + self.pat[start + 1:self.pos] + " outside of range 0-0o377", start)
                node = self.node(N_LIT)
                node.ch = chr(v)
                return node
            gi = ord(e) - 48
            if self.pos < self.n and _is_digit(self.pat[self.pos]):
                gi = gi * 10 + ord(self.pat[self.pos]) - 48
                self.pos += 1
            if gi > self.ngroups:
                self.fail("invalid group reference " + str(gi), start + 1)
            return self.backref(gi, start)
        node = self.node(N_LIT)
        node.ch = chr(self.char_escape(e, start, False))
        return node


def _width(node):
    # The fixed width of a subpattern in characters, or -1 if it varies.
    k = node.kind
    if k == N_LIT:
        return len(node.ch)
    if k == N_ANY or k == N_CLASS:
        return 1
    if k == N_BOL or k == N_EOL or k == N_WORDB or k == N_BOS or k == N_EOS or k == N_LOOK:
        return 0
    if k == N_CONCAT:
        w = 0
        for kid in node.kids:
            kw = _width(kid)
            if kw < 0:
                return -1
            w += kw
        return w
    if k == N_ALT:
        w = -2
        for kid in node.kids:
            kw = _width(kid)
            if kw < 0:
                return -1
            if w == -2:
                w = kw
            elif w != kw:
                return -1
        if w < 0:
            return 0
        return w
    if k == N_GROUP:
        return _width(node.kids[0])
    if k == N_REPEAT:
        if node.rmax == node.rmin:
            kw = _width(node.kids[0])
            if kw < 0:
                return -1
            return kw * node.rmin
        return -1
    return -1


class _ReProg:
    def __init__(self):
        self.ops = []
        self.nloops = 0

    def emit(self, kind):
        op = _ReOp(kind)
        self.ops.append(op)
        return len(self.ops) - 1


def _emit_star(prog, body, greedy):
    slot = prog.nloops
    prog.nloops += 1
    i = prog.emit(OP_LOOPINIT)
    prog.ops[i].slot = slot
    head = prog.emit(OP_LOOP)
    prog.ops[head].slot = slot
    prog.ops[head].neg = not greedy
    prog.ops[head].a = len(prog.ops)
    _emit_node(prog, body)
    j = prog.emit(OP_JMP)
    prog.ops[j].a = head
    prog.ops[head].b = len(prog.ops)


def _emit_opt(prog, body, greedy):
    split_idx = prog.emit(OP_SPLIT)
    first = len(prog.ops)
    _emit_node(prog, body)
    end = len(prog.ops)
    if greedy:
        prog.ops[split_idx].a = first
        prog.ops[split_idx].b = end
    else:
        prog.ops[split_idx].a = end
        prog.ops[split_idx].b = first


def _emit_node(prog, node):
    k = node.kind
    icase = (node.flags & IGNORECASE) != 0
    if k == N_LIT:
        for c in node.ch:
            i = prog.emit(OP_CHAR)
            prog.ops[i].ch = c
            prog.ops[i].icase = icase
            prog.ops[i].a = _fold(ord(c))
    elif k == N_ANY:
        i = prog.emit(OP_ANY)
        prog.ops[i].neg = (node.flags & DOTALL) != 0
    elif k == N_CLASS:
        i = prog.emit(OP_CLASS)
        prog.ops[i].lo = node.cls.lo
        prog.ops[i].hi = node.cls.hi
        prog.ops[i].neg = node.cls.neg
        prog.ops[i].icase = node.cls.icase
    elif k == N_BOL:
        i = prog.emit(OP_BOL)
        prog.ops[i].neg = (node.flags & MULTILINE) != 0
    elif k == N_EOL:
        i = prog.emit(OP_EOL)
        prog.ops[i].neg = (node.flags & MULTILINE) != 0
    elif k == N_WORDB:
        i = prog.emit(OP_WORDB)
        prog.ops[i].neg = node.neg
    elif k == N_BOS:
        prog.emit(OP_BOS)
    elif k == N_EOS:
        prog.emit(OP_EOS)
    elif k == N_BACKREF:
        i = prog.emit(OP_BACKREF)
        prog.ops[i].a = node.group_index
        prog.ops[i].icase = icase
    elif k == N_CONCAT:
        for kid in node.kids:
            _emit_node(prog, kid)
    elif k == N_ALT:
        jmp_ends = []
        nb = len(node.kids)
        i = 0
        while i < nb:
            if i < nb - 1:
                split_idx = prog.emit(OP_SPLIT)
                prog.ops[split_idx].a = len(prog.ops)
                _emit_node(prog, node.kids[i])
                jmp_ends.append(prog.emit(OP_JMP))
                prog.ops[split_idx].b = len(prog.ops)
            else:
                _emit_node(prog, node.kids[i])
            i += 1
        end = len(prog.ops)
        for j in jmp_ends:
            prog.ops[j].a = end
    elif k == N_GROUP:
        if node.group_index >= 0:
            i = prog.emit(OP_SAVE)
            prog.ops[i].slot = 2 * node.group_index
        _emit_node(prog, node.kids[0])
        if node.group_index >= 0:
            i = prog.emit(OP_SAVE)
            prog.ops[i].slot = 2 * node.group_index + 1
    elif k == N_LOOK:
        i = prog.emit(OP_LOOK)
        prog.ops[i].neg = node.neg
        if node.behind:
            prog.ops[i].slot = node.rmin
        else:
            prog.ops[i].slot = -1
        _emit_node(prog, node.kids[0])
        prog.emit(OP_LOOKEND)
        prog.ops[i].a = len(prog.ops)
    elif k == N_STAR:
        _emit_star(prog, node.kids[0], node.greedy)
    elif k == N_PLUS:
        _emit_node(prog, node.kids[0])
        _emit_star(prog, node.kids[0], node.greedy)
    elif k == N_OPT:
        _emit_opt(prog, node.kids[0], node.greedy)
    elif k == N_REPEAT:
        i = 0
        while i < node.rmin:
            _emit_node(prog, node.kids[0])
            i += 1
        if node.rmax < 0:
            _emit_star(prog, node.kids[0], node.greedy)
        else:
            j = node.rmin
            while j < node.rmax:
                _emit_opt(prog, node.kids[0], node.greedy)
                j += 1


class _State:
    # One search: the subject, its bounds, the capture slots, and the rules
    # at the final MATCH (fullmatch's end, an empty match not allowed at
    # the start after a previous empty match).
    def __init__(self, s, start, end, nslots, nloops):
        self.s = s
        self.start = start
        self.end = end
        self.groups = [-1] * nslots
        self.lpos = [-1] * (nloops + 1)
        self.lastindex = -1
        self.full = False
        self.must_advance = False


def _match_from(prog, pc, st, pos):
    s = st.s
    end = st.end
    while True:
        op = prog[pc]
        k = op.kind
        if k == OP_CHAR:
            if pos < end:
                if s[pos] == op.ch or (op.icase and _fold(ord(s[pos])) == op.a):
                    pos += 1
                    pc += 1
                    continue
            return -1
        elif k == OP_ANY:
            if pos < end and (op.neg or s[pos] != '\n'):
                pos += 1
                pc += 1
                continue
            return -1
        elif k == OP_CLASS:
            if pos < end and _class_match(op, s[pos]):
                pos += 1
                pc += 1
                continue
            return -1
        elif k == OP_SPLIT:
            r = _match_from(prog, op.a, st, pos)
            if r >= 0:
                return r
            pc = op.b
            continue
        elif k == OP_JMP:
            pc = op.a
            continue
        elif k == OP_LOOP:
            # A loop's head. An iteration that consumed nothing ends it, so
            # a body that can match empty cannot loop forever.
            old = st.lpos[op.slot]
            if pos == old:
                pc = op.b
                continue
            if op.neg:
                r = _match_from(prog, op.b, st, pos)
                if r >= 0:
                    return r
                st.lpos[op.slot] = pos
                r = _match_from(prog, op.a, st, pos)
                st.lpos[op.slot] = old
                return r
            st.lpos[op.slot] = pos
            r = _match_from(prog, op.a, st, pos)
            st.lpos[op.slot] = old
            if r >= 0:
                return r
            pc = op.b
            continue
        elif k == OP_LOOPINIT:
            old = st.lpos[op.slot]
            st.lpos[op.slot] = -1
            r = _match_from(prog, pc + 1, st, pos)
            st.lpos[op.slot] = old
            return r
        elif k == OP_SAVE:
            saved = st.groups[op.slot]
            saved_last = st.lastindex
            st.groups[op.slot] = pos
            if op.slot % 2 == 1:
                st.lastindex = op.slot // 2
            r = _match_from(prog, pc + 1, st, pos)
            if r < 0:
                st.groups[op.slot] = saved
                st.lastindex = saved_last
            return r
        elif k == OP_BOL:
            if pos == 0 or (op.neg and pos <= end and s[pos - 1] == '\n'):
                pc += 1
                continue
            return -1
        elif k == OP_EOL:
            if pos == end or (pos == end - 1 and s[pos] == '\n') or (op.neg and pos < end and s[pos] == '\n'):
                pc += 1
                continue
            return -1
        elif k == OP_WORDB:
            before = pos > 0 and _is_word_char(s[pos - 1])
            after = pos < end and _is_word_char(s[pos])
            if (before != after) != op.neg:
                pc += 1
                continue
            return -1
        elif k == OP_BOS:
            if pos == 0:
                pc += 1
                continue
            return -1
        elif k == OP_EOS:
            if pos == end:
                pc += 1
                continue
            return -1
        elif k == OP_BACKREF:
            gs = st.groups[2 * op.a]
            ge = st.groups[2 * op.a + 1]
            if gs < 0 or ge < 0:
                return -1
            n = ge - gs
            if pos + n > end:
                return -1
            j = 0
            while j < n:
                c1 = s[gs + j]
                c2 = s[pos + j]
                if c1 != c2 and not (op.icase and _fold(ord(c1)) == _fold(ord(c2))):
                    return -1
                j += 1
            pos += n
            pc += 1
            continue
        elif k == OP_LOOK:
            at = pos
            if op.slot >= 0:
                at = pos - op.slot
            snapshot = list(st.groups)
            ok = False
            if at >= 0:
                r = _match_from(prog, pc + 1, st, at)
                ok = r >= 0 and (op.slot < 0 or r == pos)
            if op.neg:
                st.groups = snapshot
                if ok:
                    return -1
            elif not ok:
                st.groups = snapshot
                return -1
            pc = op.a
            continue
        elif k == OP_LOOKEND:
            return pos
        else:
            # OP_MATCH
            if st.full and pos != end:
                return -1
            if st.must_advance and pos == st.start:
                return -1
            return pos


def _repr_str(s):
    return repr(s)


class Match:
    def __init__(self, st, pattern, pos, endpos):
        self.string = st.s
        self.re = pattern
        self.pos = pos
        self.endpos = endpos
        self._g = st.groups
        self._last = st.lastindex

    @property
    def lastindex(self):
        # CPython's is None when no group matched; pyc has no representation
        # for an int that may be None, so that case is refused, not -1.
        if self._last < 0:
            raise NotImplementedError("pyc: Match.lastindex is None here (no group matched), and an int that may be None has no representation in pyc (issues/210)")
        return self._last

    @property
    def lastgroup(self):
        if self._last >= 0:
            for name in self.re.groupindex:
                if self.re.groupindex[name] == self._last:
                    return name
        return None

    def _index(self, g):
        if isinstance(g, str):
            if g not in self.re.groupindex:
                raise IndexError("no such group")
            return self.re.groupindex[g]
        if g < 0 or g > self.re.groups:
            raise IndexError("no such group")
        return g

    def start(self, g=0):
        return self._g[2 * self._index(g)]

    def end(self, g=0):
        return self._g[2 * self._index(g) + 1]

    def span(self, g=0):
        i = self._index(g)
        return (self._g[2 * i], self._g[2 * i + 1])

    def group(self, g=0):
        i = self._index(g)
        st = self._g[2 * i]
        en = self._g[2 * i + 1]
        if st < 0 or en < 0:
            return None
        return self.string[st:en]

    def __getitem__(self, g):
        return self.group(g)

    def groups(self, default=None):
        r = []
        i = 1
        while i <= self.re.groups:
            st = self._g[2 * i]
            en = self._g[2 * i + 1]
            if st < 0 or en < 0:
                r.append(default)
            else:
                r.append(self.string[st:en])
            i += 1
        return tuple(r)

    def groupdict(self, default=None):
        d = {}
        for name in self.re.groupindex:
            i = self.re.groupindex[name]
            st = self._g[2 * i]
            en = self._g[2 * i + 1]
            if st < 0 or en < 0:
                d[name] = default
            else:
                d[name] = self.string[st:en]
        return d

    def expand(self, template):
        return _Template(template, self.re).expand(self)

    def __repr__(self):
        return "<re.Match object; span=(" + str(self._g[0]) + ", " + str(self._g[1]) + "), match=" + _repr_str(self.string[self._g[0]:self._g[1]]) + ">"

    def __str__(self):
        return self.__repr__()


class _Template:
    # A sub() replacement string, parsed once: literal pieces and group
    # references, with CPython's escapes.
    def __init__(self, t, pattern):
        self.lits = []
        self.refs = []
        n = len(t)
        cur = ''
        i = 0
        while i < n:
            c = t[i]
            if c != '\\':
                cur = cur + c
                i += 1
                continue
            at = i
            i += 1
            if i >= n:
                raise error("bad escape (end of pattern)", t, at)
            c = t[i]
            i += 1
            gi = -1
            if c == 'g':
                if i >= n or t[i] != '<':
                    raise error("missing <", t, i)
                j = i + 1
                name = ''
                while j < n and t[j] != '>':
                    name = name + t[j]
                    j += 1
                if j >= n:
                    raise error("missing >, unterminated name", t, i + 1)
                if name == '':
                    raise error("missing group name", t, i + 1)
                i = j + 1
                if name.isdigit():
                    gi = int(name)
                    if gi > pattern.groups:
                        raise error("invalid group reference " + name, t, at + 3)
                elif name.isidentifier():
                    if name not in pattern.groupindex:
                        raise IndexError("unknown group name '" + name + "'")
                    gi = pattern.groupindex[name]
                else:
                    raise error("bad character in group name '" + name + "'", t, at + 3)
            elif c == '0':
                v = 0
                k = 0
                while k < 2 and i < n and _is_octal(t[i]):
                    v = v * 8 + ord(t[i]) - 48
                    i += 1
                    k += 1
                cur = cur + chr(v)
                continue
            elif _is_digit(c):
                if i + 1 < n and _is_octal(c) and _is_octal(t[i]) and _is_octal(t[i + 1]):
                    v = (ord(c) - 48) * 64 + (ord(t[i]) - 48) * 8 + ord(t[i + 1]) - 48
                    i += 2
                    if v > 255:
                        raise error("octal escape value \\" + t[at + 1:i] + " outside of range 0-0o377", t, at)
                    cur = cur + chr(v)
                    continue
                gi = ord(c) - 48
                if i < n and _is_digit(t[i]):
                    gi = gi * 10 + ord(t[i]) - 48
                    i += 1
                if gi > pattern.groups:
                    raise error("invalid group reference " + str(gi), t, at + 1)
            elif c == 'n':
                cur = cur + '\n'
                continue
            elif c == 't':
                cur = cur + '\t'
                continue
            elif c == 'r':
                cur = cur + '\r'
                continue
            elif c == 'f':
                cur = cur + '\f'
                continue
            elif c == 'v':
                cur = cur + '\v'
                continue
            elif c == 'a':
                cur = cur + '\a'
                continue
            elif c == 'b':
                cur = cur + '\b'
                continue
            elif c == '\\':
                cur = cur + '\\'
                continue
            elif _is_ascii_letter(c):
                raise error("bad escape \\" + c, t, at)
            else:
                cur = cur + '\\' + c
                continue
            if cur != '':
                self.lits.append(cur)
                self.refs.append(-1)
                cur = ''
            self.lits.append('')
            self.refs.append(gi)
        if cur != '':
            self.lits.append(cur)
            self.refs.append(-1)

    def expand(self, m):
        parts = []
        i = 0
        while i < len(self.refs):
            gi = self.refs[i]
            if gi < 0:
                parts.append(self.lits[i])
            else:
                g = m.group(gi)
                if g is not None:
                    parts.append(g)
            i += 1
        return "".join(parts)


def _flags_repr(flags):
    names = []
    bit = 1
    i = 0
    while i < len(_FLAG_NAMES):
        if (flags & bit) != 0:
            names.append(_FLAG_NAMES[i])
        bit = bit * 2
        i += 1
    return "|".join(names)


class Pattern:
    def __init__(self, pattern, flags):
        if (flags & LOCALE) != 0:
            raise ValueError("cannot use LOCALE flag with a str pattern")
        if (flags & ASCII) != 0 and (flags & UNICODE) != 0:
            raise ValueError("ASCII and UNICODE flags are incompatible")
        self.pattern = pattern
        self._given = flags
        parser = _ReParser(pattern, flags)
        tree = parser.parse()
        flags = parser.flags
        if (flags & ASCII) == 0:
            flags = flags | UNICODE
        self.flags = flags
        self.groups = parser.ngroups
        self.groupindex = parser.names
        prog = _ReProg()
        _emit_node(prog, tree)
        prog.emit(OP_MATCH)
        self._ops = prog.ops
        self._nloops = prog.nloops

    def __repr__(self):
        f = self._given & ~UNICODE
        if f == 0:
            return "re.compile(" + _repr_str(self.pattern) + ")"
        return "re.compile(" + _repr_str(self.pattern) + ", " + _flags_repr(f) + ")"

    def __str__(self):
        return self.__repr__()

    def _bounds(self, s, pos, endpos):
        n = len(s)
        if endpos > n:
            endpos = n
        if endpos < 0:
            endpos = 0
        if pos < 0:
            pos = 0
        return pos, endpos

    def _attempt(self, st, at):
        # One match attempt at position `at`; True on success, with the
        # captures in st.groups.
        r = _match_from(self._ops, 0, st, at)
        if r < 0:
            return False
        st.groups[0] = at
        st.groups[1] = r
        return True

    def _search(self, s, pos, end, must_advance):
        # The leftmost match starting at or after pos, or None.
        st = _State(s, pos, end, 2 * (self.groups + 1), self._nloops)
        st.must_advance = must_advance
        at = pos
        while at <= end:
            if self._attempt(st, at):
                return st
            at += 1
        return None

    def match(self, string, pos=0, endpos=_MAXSIZE):
        pos, end = self._bounds(string, pos, endpos)
        if pos > end:
            return None
        st = _State(string, pos, end, 2 * (self.groups + 1), self._nloops)
        if self._attempt(st, pos):
            return Match(st, self, pos, end)
        return None

    def fullmatch(self, string, pos=0, endpos=_MAXSIZE):
        pos, end = self._bounds(string, pos, endpos)
        if pos > end:
            return None
        st = _State(string, pos, end, 2 * (self.groups + 1), self._nloops)
        st.full = True
        if self._attempt(st, pos):
            return Match(st, self, pos, end)
        return None

    def search(self, string, pos=0, endpos=_MAXSIZE):
        pos, end = self._bounds(string, pos, endpos)
        if pos > end:
            return None
        st = self._search(string, pos, end, False)
        if st is None:
            return None
        return Match(st, self, pos, end)

    def finditer(self, string, pos=0, endpos=_MAXSIZE):
        pos, end = self._bounds(string, pos, endpos)
        p0 = pos
        must = False
        while pos <= end:
            st = self._search(string, pos, end, must)
            if st is None:
                return
            yield Match(st, self, p0, end)
            must = st.groups[1] == st.groups[0]
            pos = st.groups[1]

    def findall(self, string, pos=0, endpos=_MAXSIZE):
        if self.groups >= 2:
            raise NotImplementedError("pyc: findall() with " + str(self.groups) + " groups returns tuples, which pyc cannot type together with the string results of fewer groups (issues/210); use [m.groups() for m in re.finditer(...)]")
        pos, end = self._bounds(string, pos, endpos)
        r = []
        must = False
        while pos <= end:
            st = self._search(string, pos, end, must)
            if st is None:
                break
            if self.groups == 0:
                r.append(string[st.groups[0]:st.groups[1]])
            elif st.groups[2] < 0 or st.groups[3] < 0:
                r.append('')
            else:
                r.append(string[st.groups[2]:st.groups[3]])
            must = st.groups[1] == st.groups[0]
            pos = st.groups[1]
        return r

    def split(self, string, maxsplit=0):
        r = []
        last = 0
        n = 0
        pos = 0
        end = len(string)
        must = False
        while maxsplit <= 0 or n < maxsplit:
            st = self._search(string, pos, end, must)
            if st is None:
                break
            b = st.groups[0]
            e = st.groups[1]
            r.append(string[last:b])
            g = 1
            while g <= self.groups:
                gs = st.groups[2 * g]
                ge = st.groups[2 * g + 1]
                if gs < 0 or ge < 0:
                    r.append(None)
                else:
                    r.append(string[gs:ge])
                g += 1
            last = e
            n += 1
            must = b == e
            pos = e
        r.append(string[last:])
        return r

    def _sub_template(self, t, string, count):
        parts = []
        last = 0
        n = 0
        pos = 0
        end = len(string)
        must = False
        while count <= 0 or n < count:
            st = self._search(string, pos, end, must)
            if st is None:
                break
            b = st.groups[0]
            e = st.groups[1]
            parts.append(string[last:b])
            parts.append(t.expand(Match(st, self, 0, end)))
            last = e
            n += 1
            must = b == e
            pos = e
        parts.append(string[last:])
        return ("".join(parts), n)

    def _sub_call(self, fn, string, count):
        parts = []
        last = 0
        n = 0
        pos = 0
        end = len(string)
        must = False
        while count <= 0 or n < count:
            st = self._search(string, pos, end, must)
            if st is None:
                break
            b = st.groups[0]
            e = st.groups[1]
            parts.append(string[last:b])
            parts.append(fn(Match(st, self, 0, end)))
            last = e
            n += 1
            must = b == e
            pos = e
        parts.append(string[last:])
        return ("".join(parts), n)

    def subn(self, repl, string, count=0):
        if isinstance(repl, str):
            return self._sub_template(_Template(repl, self), string, count)
        return self._sub_call(repl, string, count)

    def sub(self, repl, string, count=0):
        if isinstance(repl, str):
            return self._sub_template(_Template(repl, self), string, count)[0]
        return self._sub_call(repl, string, count)[0]


# --- the module functions ----------------------------------------------------

_cache = {}


def _compile(pattern, flags):
    if isinstance(pattern, Pattern):
        if flags != 0:
            raise ValueError("cannot process flags argument with a compiled pattern")
        return pattern
    key = str(flags) + ":" + pattern
    if key in _cache:
        return _cache[key]
    p = Pattern(pattern, flags)
    if len(_cache) >= 512:
        _cache.clear()
    _cache[key] = p
    return p


def compile(pattern, flags=0):
    return _compile(pattern, flags)


def search(pattern, string, flags=0):
    return _compile(pattern, flags).search(string)


def match(pattern, string, flags=0):
    return _compile(pattern, flags).match(string)


def fullmatch(pattern, string, flags=0):
    return _compile(pattern, flags).fullmatch(string)


def findall(pattern, string, flags=0):
    return _compile(pattern, flags).findall(string)


def finditer(pattern, string, flags=0):
    return _compile(pattern, flags).finditer(string)


def split(pattern, string, maxsplit=0, flags=0):
    return _compile(pattern, flags).split(string, maxsplit)


def sub(pattern, repl, string, count=0, flags=0):
    return _compile(pattern, flags).sub(repl, string, count)


def subn(pattern, repl, string, count=0, flags=0):
    return _compile(pattern, flags).subn(repl, string, count)


_SPECIAL = "()[]{}?*+-|^$\\.&~# \t\n\r\v\f"


def escape(pattern):
    parts = []
    for c in pattern:
        if c in _SPECIAL:
            parts.append('\\')
        parts.append(c)
    return "".join(parts)


def purge():
    _cache.clear()
