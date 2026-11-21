# pyc shim for the standard `textwrap` module (issues/205).
#
# A port of CPython 3.12's textwrap: TextWrapper (wrap, fill), wrap, fill,
# shorten, dedent and indent, with the same results. CPython splits text
# into chunks with one regex (wordsep_re); _split_hyphenated below is that
# regex's matcher written out by hand, alternative by alternative, so the
# chunks are the ones CPython makes.
#
# wrap(), fill() and shorten() take TextWrapper's keywords by name rather
# than through **kwargs, which pyc refuses when the values have mixed types
# (issues/189). Characters are counted as pyc's str counts them, in UTF-8
# bytes (issues/203), so non-ASCII text wraps short.

_whitespace = '\t\n\x0b\x0c\r '


def _is_ws(c):
    return c in _whitespace


def _is_word(c):
    return c.isalnum() or c == '_'


# `[^\d\W]`: a word character that is not a digit.
def _is_letter(c):
    return c.isalpha() or c == '_'


# `[\w!"\'&.,?]`
def _is_word_punct(c):
    return _is_word(c) or c in '!"\'&.,?'


def _at(text, k):
    if k < 0 or k >= len(text):
        return ''
    return text[k]


def _letter_at(text, k):
    c = _at(text, k)
    return c != '' and _is_letter(c)


# Alternative 2 of wordsep_re at i: `(?<=wp) -{2,} (?=\w)`. The end of the
# match, or -1.
def _em_dash_at(text, i):
    if i == 0 or not _is_word_punct(text[i - 1]):
        return -1
    j = i
    while j < len(text) and text[j] == '-':
        j += 1
    if j - i < 2 or j >= len(text) or not _is_word(text[j]):
        return -1
    return j


# Alternative 3 of wordsep_re at i: `nws+?` (lazy) followed by a hyphen
# inside a compound word (consumed), the end of the word, or an em-dash.
def _word_at(text, i):
    n = len(text)
    j = i + 1
    while j <= n:
        # a: `-(?:(?<=lt{2}-)|(?<=lt-lt-))(?=lt-?lt)`
        if j < n and text[j] == '-':
            behind = (_letter_at(text, j - 2) and _letter_at(text, j - 1)) or \
                     (_letter_at(text, j - 3) and _at(text, j - 2) == '-' and _letter_at(text, j - 1))
            ahead = _letter_at(text, j + 1) and \
                (_letter_at(text, j + 2) or (_at(text, j + 2) == '-' and _letter_at(text, j + 3)))
            if behind and ahead:
                return j + 1
        # b: `(?=ws|\Z)`
        if j == n or _is_ws(text[j]):
            return j
        # c: `(?<=wp)(?=-{2,}\w)`
        if _is_word_punct(text[j - 1]):
            k = j
            while k < n and text[k] == '-':
                k += 1
            if k - j >= 2 and k < n and _is_word(text[k]):
                return j
        if _is_ws(text[j]):
            break
        j += 1
    return n


def _split_hyphenated(text):
    chunks = []
    i = 0
    n = len(text)
    while i < n:
        if _is_ws(text[i]):
            j = i
            while j < n and _is_ws(text[j]):
                j += 1
        else:
            j = _em_dash_at(text, i)
            if j < 0:
                j = _word_at(text, i)
        chunks.append(text[i:j])
        i = j
    return chunks


def _split_simple(text):
    chunks = []
    i = 0
    n = len(text)
    while i < n:
        ws = _is_ws(text[i])
        j = i
        while j < n and _is_ws(text[j]) == ws:
            j += 1
        chunks.append(text[i:j])
        i = j
    return chunks


# sentence_end_re: `[a-z][\.\!\?][\"\']?\Z`
def _sentence_end(chunk):
    n = len(chunk)
    k = n - 1
    if k >= 0 and (chunk[k] == '"' or chunk[k] == "'"):
        k -= 1
    if k < 1:
        return False
    return chunk[k] in '.!?' and 'a' <= chunk[k - 1] and chunk[k - 1] <= 'z'


class TextWrapper:
    def __init__(self, width=70, initial_indent="", subsequent_indent="",
                 expand_tabs=True, replace_whitespace=True,
                 fix_sentence_endings=False, break_long_words=True,
                 drop_whitespace=True, break_on_hyphens=True, tabsize=8,
                 *, max_lines=None, placeholder=' [...]'):
        self.width = width
        self.initial_indent = initial_indent
        self.subsequent_indent = subsequent_indent
        self.expand_tabs = expand_tabs
        self.replace_whitespace = replace_whitespace
        self.fix_sentence_endings = fix_sentence_endings
        self.break_long_words = break_long_words
        self.drop_whitespace = drop_whitespace
        self.break_on_hyphens = break_on_hyphens
        self.tabsize = tabsize
        # Kept as a flag and an int, so the field is not {None, int}.
        self._has_max_lines = max_lines is not None
        self._max_lines = 0 if max_lines is None else max_lines
        self.placeholder = placeholder

    def _munge_whitespace(self, text):
        if self.expand_tabs:
            text = text.expandtabs(self.tabsize)
        if self.replace_whitespace:
            out = []
            for c in text:
                out.append(' ' if _is_ws(c) else c)
            text = ''.join(out)
        return text

    def _split(self, text):
        if self.break_on_hyphens:
            chunks = _split_hyphenated(text)
        else:
            chunks = _split_simple(text)
        return [c for c in chunks if c]

    def _fix_sentence_endings(self, chunks):
        i = 0
        while i < len(chunks) - 1:
            if chunks[i + 1] == " " and _sentence_end(chunks[i]):
                chunks[i + 1] = "  "
                i += 2
            else:
                i += 1

    def _handle_long_word(self, reversed_chunks, cur_line, cur_len, width):
        if width < 1:
            space_left = 1
        else:
            space_left = width - cur_len
        if self.break_long_words:
            end = space_left
            chunk = reversed_chunks[-1]
            if self.break_on_hyphens and len(chunk) > space_left:
                hyphen = chunk.rfind('-', 0, space_left)
                if hyphen > 0:
                    k = 0
                    while k < hyphen and chunk[k] == '-':
                        k += 1
                    if k < hyphen:
                        end = hyphen + 1
            cur_line.append(chunk[:end])
            reversed_chunks[-1] = chunk[end:]
        elif not cur_line:
            cur_line.append(reversed_chunks.pop())

    def _wrap_chunks(self, chunks):
        lines = []
        if self.width <= 0:
            raise ValueError("invalid width %r (must be > 0)" % self.width)
        max_lines = self._max_lines
        has_max = self._has_max_lines
        if has_max:
            if max_lines > 1:
                indent = self.subsequent_indent
            else:
                indent = self.initial_indent
            if len(indent) + len(self.placeholder.lstrip()) > self.width:
                raise ValueError("placeholder too large for max width")
        chunks.reverse()
        while chunks:
            cur_line = []
            cur_len = 0
            if lines:
                indent = self.subsequent_indent
            else:
                indent = self.initial_indent
            width = self.width - len(indent)
            if self.drop_whitespace and chunks[-1].strip() == '' and lines:
                del chunks[-1]
            while chunks:
                l = len(chunks[-1])
                if cur_len + l <= width:
                    cur_line.append(chunks.pop())
                    cur_len += l
                else:
                    break
            if chunks and len(chunks[-1]) > width:
                self._handle_long_word(chunks, cur_line, cur_len, width)
                cur_len = 0
                for c in cur_line:
                    cur_len += len(c)
            if self.drop_whitespace and cur_line and cur_line[-1].strip() == '':
                cur_len -= len(cur_line[-1])
                del cur_line[-1]
            if cur_line:
                if (not has_max or
                        len(lines) + 1 < max_lines or
                        (not chunks or
                         self.drop_whitespace and
                         len(chunks) == 1 and
                         not chunks[0].strip()) and cur_len <= width):
                    lines.append(indent + ''.join(cur_line))
                else:
                    placed = False
                    while cur_line:
                        if (cur_line[-1].strip() and
                                cur_len + len(self.placeholder) <= width):
                            cur_line.append(self.placeholder)
                            lines.append(indent + ''.join(cur_line))
                            placed = True
                            break
                        cur_len -= len(cur_line[-1])
                        del cur_line[-1]
                    if not placed:
                        if lines:
                            prev_line = lines[-1].rstrip()
                            if len(prev_line) + len(self.placeholder) <= self.width:
                                lines[-1] = prev_line + self.placeholder
                                break
                        lines.append(indent + self.placeholder.lstrip())
                    break
        return lines

    def _split_chunks(self, text):
        text = self._munge_whitespace(text)
        return self._split(text)

    def wrap(self, text):
        chunks = self._split_chunks(text)
        if self.fix_sentence_endings:
            self._fix_sentence_endings(chunks)
        return self._wrap_chunks(chunks)

    def fill(self, text):
        return "\n".join(self.wrap(text))


def wrap(text, width=70, initial_indent="", subsequent_indent="",
         expand_tabs=True, replace_whitespace=True, fix_sentence_endings=False,
         break_long_words=True, drop_whitespace=True, break_on_hyphens=True,
         tabsize=8, max_lines=None, placeholder=' [...]'):
    w = TextWrapper(width, initial_indent, subsequent_indent, expand_tabs,
                    replace_whitespace, fix_sentence_endings, break_long_words,
                    drop_whitespace, break_on_hyphens, tabsize,
                    max_lines=max_lines, placeholder=placeholder)
    return w.wrap(text)


def fill(text, width=70, initial_indent="", subsequent_indent="",
         expand_tabs=True, replace_whitespace=True, fix_sentence_endings=False,
         break_long_words=True, drop_whitespace=True, break_on_hyphens=True,
         tabsize=8, max_lines=None, placeholder=' [...]'):
    w = TextWrapper(width, initial_indent, subsequent_indent, expand_tabs,
                    replace_whitespace, fix_sentence_endings, break_long_words,
                    drop_whitespace, break_on_hyphens, tabsize,
                    max_lines=max_lines, placeholder=placeholder)
    return w.fill(text)


def shorten(text, width, initial_indent="", subsequent_indent="",
            expand_tabs=True, replace_whitespace=True, fix_sentence_endings=False,
            break_long_words=True, drop_whitespace=True, break_on_hyphens=True,
            tabsize=8, placeholder=' [...]'):
    w = TextWrapper(width, initial_indent, subsequent_indent, expand_tabs,
                    replace_whitespace, fix_sentence_endings, break_long_words,
                    drop_whitespace, break_on_hyphens, tabsize,
                    max_lines=1, placeholder=placeholder)
    return w.fill(' '.join(text.strip().split()))


# dedent: CPython blanks the lines of only spaces and tabs (`^[ \t]+$`),
# takes the leading `[ \t]*` of every line holding anything else, narrows
# the margin to their common prefix, and removes it from each line that
# starts with it.
def dedent(text):
    lines = text.split('\n')
    k = 0
    while k < len(lines):
        line = lines[k]
        if line and line.strip(' \t') == '':
            lines[k] = ''
        k += 1
    margin = ''
    have_margin = False
    for line in lines:
        j = 0
        while j < len(line) and (line[j] == ' ' or line[j] == '\t'):
            j += 1
        if j == len(line):
            continue
        indent = line[:j]
        if not have_margin:
            margin = indent
            have_margin = True
        elif indent.startswith(margin):
            pass
        elif margin.startswith(indent):
            margin = indent
        else:
            i = 0
            while i < len(margin) and i < len(indent):
                if margin[i] != indent[i]:
                    margin = margin[:i]
                    break
                i += 1
    if margin:
        k = 0
        while k < len(lines):
            if lines[k].startswith(margin):
                lines[k] = lines[k][len(margin):]
            k += 1
    return '\n'.join(lines)


def indent(text, prefix, predicate=None):
    out = []
    if predicate is None:
        for line in text.splitlines(True):
            out.append(prefix + line if line.strip() else line)
    else:
        for line in text.splitlines(True):
            out.append(prefix + line if predicate(line) else line)
    return ''.join(out)
