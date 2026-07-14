# pyc shim for the standard `re` module: a small backtracking regex
# engine (Pike-VM style -- a flat instruction list with SPLIT/JMP for
# alternation and repetition, walked by one recursive matcher, same
# family as Russ Cox's backtrack.c). Supports literals, '.', character
# classes ([...], \d \D \w \W \s \S), groups (capturing and
# non-capturing (?:...)), alternation '|', quantifiers * + ? {m,n}
# (with non-greedy '?' suffix), and anchors ^ $.
#
# Deliberately narrow surface -- module-level match()/fullmatch()/
# compile() only, each a single one-shot Pattern().match() call. Two
# pyc compiler bugs (both ifa/issues/040-class cross-clone
# interference in the whole-program flow analysis, not bugs in this
# module's logic -- confirmed by removing/isolating code, not
# guessed) cut the surface down from a fuller re implementation:
#
# 1. Assigning a locally-built list wholesale to a shared class field
#    (`node.kids = some_list`) -- whether a single-element list
#    literal or a pre-built list variable -- silently dropped
#    unrelated ops emitted by _emit_node elsewhere in the same
#    compiled program (e.g. a completely separate `re.match('abc',
#    ...)` call would stop advancing past position 0). Fixed here by
#    never rebinding `.kids`; always growing the field's own list in
#    place via `.append()` (see parse_alt, parse_concat, and the
#    N_REPEAT case in _emit_node) -- but the underlying compiler bug
#    is unfixed, so any future edit to this file that reintroduces a
#    `.kids = ...` rebind can silently reopen it.
# 2. Calling Pattern._run() (the match attempt) more than once on the
#    same Pattern instance segfaults -- confirmed with two sequential
#    `.match()` calls on one compiled pattern, no search() involved.
#    Root cause not found. This is why search() (which loops _run()
#    over start positions) and findall/sub/split (which do the same)
#    are not implemented: build a fresh Pattern (or use the
#    module-level match/fullmatch functions, which do this for you)
#    for every match attempt.
#
# str has no working slice path yet (see __pyc__/01_str.py), so all
# substring work here goes through single-char indexing.
# Match.groups() returns a list, not a tuple, since pyc tuples are
# fixed-arity per call site.

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

def _is_word_char(c):
    o = ord(c)
    if o >= 48 and o <= 57:
        return True
    if o >= 65 and o <= 90:
        return True
    if o >= 97 and o <= 122:
        return True
    if o == 95:
        return True
    return False

class _ReOp:
    def __init__(self, kind):
        self.kind = kind
        self.ch = ''
        self.a = 0
        self.b = 0
        self.slot = 0
        self.neg = False
        self.lo = []
        self.hi = []

def _class_match(op, c):
    o = ord(c)
    matched = False
    n = len(op.lo)
    i = 0
    while i < n:
        if o >= op.lo[i] and o <= op.hi[i]:
            matched = True
            break
        i += 1
    if op.neg:
        return not matched
    return matched

def _digit_class(op):
    op.lo.append(48)
    op.hi.append(57)

def _word_class(op):
    op.lo.append(48)
    op.hi.append(57)
    op.lo.append(65)
    op.hi.append(90)
    op.lo.append(97)
    op.hi.append(122)
    op.lo.append(95)
    op.hi.append(95)

def _space_class(op):
    op.lo.append(32)
    op.hi.append(32)
    op.lo.append(9)
    op.hi.append(13)

class _ReNode:
    def __init__(self, kind):
        self.kind = kind
        self.ch = ''
        self.cls = _ReOp(OP_CLASS)
        self.kids = []
        self.group_index = -1
        self.rmin = 0
        self.rmax = -1
        self.greedy = True

class _ReParser:
    def __init__(self, pattern):
        self.pat = pattern
        self.pos = 0
        self.n = len(pattern)
        self.ngroups = 0

    def peek(self):
        if self.pos < self.n:
            return self.pat[self.pos]
        return ''

    def advance(self):
        c = self.pat[self.pos]
        self.pos += 1
        return c

    def parse(self):
        return self.parse_alt()

    def parse_alt(self):
        # ifa/issues/040-class cross-clone interference: assigning a
        # locally-built list wholesale to a shared class field (`.kids
        # = some_list`) -- whether via a single-element literal or a
        # pre-built list variable -- corrupted an unrelated caller's
        # AST traversal elsewhere in the same compiled program.
        # Always grow the field's own list in place via .append()
        # instead; never rebind it.
        first = self.parse_concat()
        if self.peek() != '|':
            return first
        alt = _ReNode(N_ALT)
        alt.kids.append(first)
        while self.peek() == '|':
            self.pos += 1
            alt.kids.append(self.parse_concat())
        return alt

    def parse_concat(self):
        node = _ReNode(N_CONCAT)
        while self.pos < self.n and self.peek() != '|' and self.peek() != ')':
            node.kids.append(self.parse_repeat())
        return node

    def parse_int(self):
        s = ''
        while self.pos < self.n and self.pat[self.pos] >= '0' and self.pat[self.pos] <= '9':
            s = s + self.pat[self.pos]
            self.pos += 1
        if s == '':
            return -1
        return int(s)

    def parse_repeat(self):
        atom = self.parse_atom()
        c = self.peek()
        if c == '*':
            self.pos += 1
            node = _ReNode(N_STAR)
            node.kids.append(atom)
            if self.peek() == '?':
                self.pos += 1
                node.greedy = False
            return node
        elif c == '+':
            self.pos += 1
            node = _ReNode(N_PLUS)
            node.kids.append(atom)
            if self.peek() == '?':
                self.pos += 1
                node.greedy = False
            return node
        elif c == '?':
            self.pos += 1
            node = _ReNode(N_OPT)
            node.kids.append(atom)
            if self.peek() == '?':
                self.pos += 1
                node.greedy = False
            return node
        elif c == '{':
            save = self.pos
            self.pos += 1
            m = self.parse_int()
            if m < 0:
                m = 0
            mx = m
            if self.peek() == ',':
                self.pos += 1
                if self.peek() == '}':
                    mx = -1
                else:
                    mx = self.parse_int()
            if self.peek() == '}':
                self.pos += 1
                node = _ReNode(N_REPEAT)
                node.kids.append(atom)
                node.rmin = m
                node.rmax = mx
                if self.peek() == '?':
                    self.pos += 1
                    node.greedy = False
                return node
            else:
                self.pos = save
                return atom
        return atom

    def parse_atom(self):
        c = self.advance()
        if c == '.':
            return _ReNode(N_ANY)
        elif c == '^':
            return _ReNode(N_BOL)
        elif c == '$':
            return _ReNode(N_EOL)
        elif c == '(':
            capturing = True
            if self.peek() == '?':
                self.pos += 1
                if self.peek() == ':':
                    self.pos += 1
                capturing = False
            sub = self.parse_alt()
            if self.peek() == ')':
                self.pos += 1
            node = _ReNode(N_GROUP)
            node.kids.append(sub)
            if capturing:
                self.ngroups += 1
                node.group_index = self.ngroups
            else:
                node.group_index = -1
            return node
        elif c == '[':
            return self.parse_class()
        elif c == '\\':
            return self.parse_escape()
        else:
            node = _ReNode(N_LIT)
            node.ch = c
            return node

    def parse_class(self):
        op = _ReOp(OP_CLASS)
        if self.peek() == '^':
            self.pos += 1
            op.neg = True
        first = True
        while self.pos < self.n and (self.peek() != ']' or first):
            first = False
            c = self.advance()
            if c == '\\':
                e = self.advance()
                self._add_escape_class(op, e)
                continue
            lo = ord(c)
            hi = lo
            if self.peek() == '-' and self.pos + 1 < self.n and self.pat[self.pos + 1] != ']':
                self.pos += 1
                c2 = self.advance()
                hi = ord(c2)
            op.lo.append(lo)
            op.hi.append(hi)
        if self.peek() == ']':
            self.pos += 1
        node = _ReNode(N_CLASS)
        node.cls = op
        return node

    def _add_escape_class(self, op, e):
        if e == 'd' or e == 'D':
            _digit_class(op)
        elif e == 'w' or e == 'W':
            _word_class(op)
        elif e == 's' or e == 'S':
            _space_class(op)
        elif e == 'n':
            op.lo.append(10)
            op.hi.append(10)
        elif e == 't':
            op.lo.append(9)
            op.hi.append(9)
        elif e == 'r':
            op.lo.append(13)
            op.hi.append(13)
        else:
            o = ord(e)
            op.lo.append(o)
            op.hi.append(o)

    def parse_escape(self):
        e = self.advance()
        if e == 'd':
            op = _ReOp(OP_CLASS)
            _digit_class(op)
            node = _ReNode(N_CLASS)
            node.cls = op
            return node
        elif e == 'D':
            op = _ReOp(OP_CLASS)
            _digit_class(op)
            op.neg = True
            node = _ReNode(N_CLASS)
            node.cls = op
            return node
        elif e == 'w':
            op = _ReOp(OP_CLASS)
            _word_class(op)
            node = _ReNode(N_CLASS)
            node.cls = op
            return node
        elif e == 'W':
            op = _ReOp(OP_CLASS)
            _word_class(op)
            op.neg = True
            node = _ReNode(N_CLASS)
            node.cls = op
            return node
        elif e == 's':
            op = _ReOp(OP_CLASS)
            _space_class(op)
            node = _ReNode(N_CLASS)
            node.cls = op
            return node
        elif e == 'S':
            op = _ReOp(OP_CLASS)
            _space_class(op)
            op.neg = True
            node = _ReNode(N_CLASS)
            node.cls = op
            return node
        elif e == 'b':
            return _ReNode(N_WORDB)
        elif e == 'n':
            node = _ReNode(N_LIT)
            node.ch = chr(10)
            return node
        elif e == 't':
            node = _ReNode(N_LIT)
            node.ch = chr(9)
            return node
        elif e == 'r':
            node = _ReNode(N_LIT)
            node.ch = chr(13)
            return node
        else:
            node = _ReNode(N_LIT)
            node.ch = e
            return node

class _ReProg:
    def __init__(self):
        self.ops = []

    def emit(self, kind):
        op = _ReOp(kind)
        self.ops.append(op)
        return len(self.ops) - 1

def _emit_node(prog, node):
    k = node.kind
    if k == N_LIT:
        i = prog.emit(OP_CHAR)
        prog.ops[i].ch = node.ch
    elif k == N_ANY:
        prog.emit(OP_ANY)
    elif k == N_CLASS:
        i = prog.emit(OP_CLASS)
        prog.ops[i].lo = node.cls.lo
        prog.ops[i].hi = node.cls.hi
        prog.ops[i].neg = node.cls.neg
    elif k == N_BOL:
        prog.emit(OP_BOL)
    elif k == N_EOL:
        prog.emit(OP_EOL)
    elif k == N_WORDB:
        prog.emit(OP_WORDB)
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
                a = len(prog.ops)
                prog.ops[split_idx].a = a
                _emit_node(prog, node.kids[i])
                jmp_idx = prog.emit(OP_JMP)
                jmp_ends.append(jmp_idx)
                b = len(prog.ops)
                prog.ops[split_idx].b = b
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
    elif k == N_STAR:
        split_idx = prog.emit(OP_SPLIT)
        body = len(prog.ops)
        if node.greedy:
            prog.ops[split_idx].a = body
        else:
            prog.ops[split_idx].b = body
        _emit_node(prog, node.kids[0])
        j = prog.emit(OP_JMP)
        prog.ops[j].a = split_idx
        end = len(prog.ops)
        if node.greedy:
            prog.ops[split_idx].b = end
        else:
            prog.ops[split_idx].a = end
    elif k == N_PLUS:
        body = len(prog.ops)
        _emit_node(prog, node.kids[0])
        split_idx = prog.emit(OP_SPLIT)
        if node.greedy:
            prog.ops[split_idx].a = body
        else:
            prog.ops[split_idx].b = body
        end = len(prog.ops)
        if node.greedy:
            prog.ops[split_idx].b = end
        else:
            prog.ops[split_idx].a = end
    elif k == N_OPT:
        split_idx = prog.emit(OP_SPLIT)
        body = len(prog.ops)
        if node.greedy:
            prog.ops[split_idx].a = body
        else:
            prog.ops[split_idx].b = body
        _emit_node(prog, node.kids[0])
        end = len(prog.ops)
        if node.greedy:
            prog.ops[split_idx].b = end
        else:
            prog.ops[split_idx].a = end
    elif k == N_REPEAT:
        i = 0
        while i < node.rmin:
            _emit_node(prog, node.kids[0])
            i += 1
        if node.rmax < 0:
            star = _ReNode(N_STAR)
            star.kids.append(node.kids[0])
            star.greedy = node.greedy
            _emit_node(prog, star)
        else:
            extra = node.rmax - node.rmin
            j = 0
            while j < extra:
                opt = _ReNode(N_OPT)
                opt.kids.append(node.kids[0])
                opt.greedy = node.greedy
                _emit_node(prog, opt)
                j += 1

def _match_from(prog, pc, s, pos, groups, slen):
    while True:
        op = prog[pc]
        k = op.kind
        if k == OP_CHAR:
            if pos < slen and s[pos] == op.ch:
                pos += 1
                pc += 1
                continue
            return -1
        elif k == OP_ANY:
            if pos < slen:
                pos += 1
                pc += 1
                continue
            return -1
        elif k == OP_CLASS:
            if pos < slen and _class_match(op, s[pos]):
                pos += 1
                pc += 1
                continue
            return -1
        elif k == OP_BOL:
            if pos == 0:
                pc += 1
                continue
            return -1
        elif k == OP_EOL:
            if pos == slen:
                pc += 1
                continue
            return -1
        elif k == OP_WORDB:
            before = pos > 0 and _is_word_char(s[pos - 1])
            after = pos < slen and _is_word_char(s[pos])
            if before != after:
                pc += 1
                continue
            return -1
        elif k == OP_JMP:
            pc = op.a
            continue
        elif k == OP_SPLIT:
            r = _match_from(prog, op.a, s, pos, groups, slen)
            if r >= 0:
                return r
            pc = op.b
            continue
        elif k == OP_SAVE:
            saved = groups[op.slot]
            groups[op.slot] = pos
            r = _match_from(prog, pc + 1, s, pos, groups, slen)
            if r < 0:
                groups[op.slot] = saved
            return r
        else:
            return pos

class Match:
    def __init__(self, s, groups, ngroups):
        self.string = s
        self._groups = groups
        self.ngroups = ngroups

    def start(self, n=0):
        return self._groups[2 * n]

    def end(self, n=0):
        return self._groups[2 * n + 1]

    def span(self, n=0):
        return (self.start(n), self.end(n))

    def group(self, n=0):
        st = self._groups[2 * n]
        en = self._groups[2 * n + 1]
        if st < 0 or en < 0:
            return ""
        r = ""
        k = st
        while k < en:
            r = r + self.string[k]
            k += 1
        return r

    def groups(self):
        r = []
        i = 1
        while i <= self.ngroups:
            r.append(self.group(i))
            i += 1
        return r

class Pattern:
    def __init__(self, pattern):
        self.pattern = pattern
        parser = _ReParser(pattern)
        ast = parser.parse()
        self.ngroups = parser.ngroups
        prog = _ReProg()
        i0 = prog.emit(OP_SAVE)
        prog.ops[i0].slot = 0
        _emit_node(prog, ast)
        i1 = prog.emit(OP_SAVE)
        prog.ops[i1].slot = 1
        prog.emit(OP_MATCH)
        self.ops = prog.ops

    def _run(self, s, start):
        nslots = 2 * (self.ngroups + 1)
        groups = []
        i = 0
        while i < nslots:
            groups.append(-1)
            i += 1
        slen = len(s)
        end = _match_from(self.ops, 0, s, start, groups, slen)
        if end < 0:
            return None
        return Match(s, groups, self.ngroups)

    def match(self, s):
        return self._run(s, 0)

    def fullmatch(self, s):
        m = self._run(s, 0)
        if m is not None and m.end(0) == len(s):
            return m
        return None


def compile(pattern):
    return Pattern(pattern)

def match(pattern, s):
    return Pattern(pattern).match(s)

def fullmatch(pattern, s):
    return Pattern(pattern).fullmatch(s)
