# pyc shim for the standard `logging` module (issues/197).
#
# A port of the core of CPython's logging/__init__.py: levels, LogRecord,
# Formatter ('%' style), Filter, Handler, StreamHandler, FileHandler,
# NullHandler, the logger hierarchy, basicConfig and the module-level
# functions. Output matches CPython's byte for byte for what is here.
#
# Refused, so that a program needing them fails at compile time (an
# unknown name, member or keyword) or raises NotImplementedError before
# it prints anything wrong:
#   - the caller's location in a format: %(pathname)s, %(filename)s,
#     %(module)s, %(lineno)d, %(funcName)s, and %(process)d,
#     %(thread)d and friends. CPython reads them from the stack frame;
#   - exc_info / stack_info / extra / stacklevel, Logger.exception(),
#     warn() (a DeprecationWarning in CPython);
#   - '{' and '$' format styles; a dict as the single message argument;
#   - a callable as a filter (a Filter object works);
#   - LoggerAdapter, logging.handlers, logging.config, dictConfig.
#
# The logger chain and a handler's formatter are tested `is not None`, not
# for truth as CPython's source does: the same answer for these classes,
# which define neither __bool__ nor __len__, and a truth test on a
# {None, Logger, RootLogger} union trips ifa/213 (slot elision).
#
# One deviation, on stderr only: a handler whose format raises prints
# CPython's "--- Logging error ---" report without the traceback (pyc has
# none) and carries on, as CPython does.

import sys
import time

CRITICAL = 50
FATAL = CRITICAL
ERROR = 40
WARNING = 30
WARN = WARNING
INFO = 20
DEBUG = 10
NOTSET = 0

_levelToName = {
    CRITICAL: 'CRITICAL',
    ERROR: 'ERROR',
    WARNING: 'WARNING',
    INFO: 'INFO',
    DEBUG: 'DEBUG',
    NOTSET: 'NOTSET',
}
_nameToLevel = {
    'CRITICAL': CRITICAL,
    'FATAL': FATAL,
    'ERROR': ERROR,
    'WARN': WARNING,
    'WARNING': WARNING,
    'INFO': INFO,
    'DEBUG': DEBUG,
    'NOTSET': NOTSET,
}

raiseExceptions = True

_startTime = time.time()


def getLevelName(level):
    # CPython also maps a level NAME back to its number; that answer has a
    # different type, so it is refused here rather than unioned.
    if isinstance(level, str):
        raise NotImplementedError("pyc: logging.getLevelName of a level name is not implemented")
    if level in _levelToName:
        return _levelToName[level]
    return "Level %s" % level


def addLevelName(level, levelName):
    _levelToName[level] = levelName
    _nameToLevel[levelName] = level


def _checkLevel(level):
    if isinstance(level, int):
        return level
    if isinstance(level, str):
        if level not in _nameToLevel:
            raise ValueError("Unknown level: %r" % level)
        return _nameToLevel[level]
    raise TypeError("Level not an integer or a valid string: %r" % level)


class LogRecord:
    def __init__(self, name, level, pathname, lineno, msg, args, exc_info, func=None, sinfo=None):
        ct = time.time()
        self.name = name
        self.msg = msg
        if len(args) == 1 and isinstance(args[0], dict):
            raise NotImplementedError("pyc: a dict as the logging message argument is not implemented")
        self.args = args
        self.levelname = getLevelName(level)
        self.levelno = level
        self.pathname = pathname
        self.lineno = lineno
        self.funcName = func
        self.created = ct
        self.msecs = int((ct - int(ct)) * 1000) + 0.0
        self.relativeCreated = (ct - _startTime) * 1000

    def __repr__(self):
        return '<LogRecord: %s, %s, %s, %s, "%s">' % (self.name, self.levelno, self.pathname, self.lineno, self.msg)

    def getMessage(self):
        msg = str(self.msg)
        if self.args:
            msg = msg % self.args
        return msg


# The fields a '%' format may name. CPython formats `fmt % record.__dict__`;
# pyc parses the format once and formats each field with its own spec.
_STR_FIELDS = ('name', 'levelname', 'message', 'asctime')
_INT_FIELDS = ('levelno',)
_FLOAT_FIELDS = ('created', 'msecs', 'relativeCreated')
_FRAME_FIELDS = ('pathname', 'filename', 'module', 'lineno', 'funcName',
                 'stack_info', 'exc_info', 'exc_text', 'thread', 'threadName',
                 'process', 'processName', 'taskName')
_SPEC_FLAGS = '#0+ -'
_SPEC_CONV = 'diouxXeEfFgGcrsa'


def _is_digit(c):
    return c >= '0' and c <= '9'


class _Piece:
    # A literal run, or one `%(key)spec` field.
    def __init__(self, text, key, spec):
        self.text = text
        self.key = key
        self.spec = spec


def _parse_format(fmt):
    # Returns the pieces, or None if fmt has a `%` that is not a valid
    # `%(key)spec` or `%%` (CPython's validate() then raises).
    pieces = []
    lit = ''
    n = len(fmt)
    i = 0
    nfields = 0
    while i < n:
        c = fmt[i]
        if c != '%':
            lit += c
            i += 1
            continue
        if i + 1 < n and fmt[i + 1] == '%':
            lit += '%'
            i += 2
            continue
        if i + 1 >= n or fmt[i + 1] != '(':
            return None
        j = fmt.find(')', i + 2)
        if j < 0:
            return None
        key = fmt[i + 2:j]
        k = j + 1
        while k < n and _SPEC_FLAGS.find(fmt[k]) >= 0:
            k += 1
        while k < n and _is_digit(fmt[k]):
            k += 1
        if k < n and fmt[k] == '.':
            k += 1
            while k < n and _is_digit(fmt[k]):
                k += 1
        if k >= n or _SPEC_CONV.find(fmt[k]) < 0:
            return None
        if lit != '':
            pieces.append(_Piece(lit, '', ''))
            lit = ''
        pieces.append(_Piece('', key, '%' + fmt[j + 1:k + 1]))
        nfields += 1
        i = k + 1
    if lit != '':
        pieces.append(_Piece(lit, '', ''))
    if nfields == 0:
        return None
    return pieces


class Formatter:
    default_time_format = '%Y-%m-%d %H:%M:%S'
    default_msec_format = '%s,%03d'

    def __init__(self, fmt=None, datefmt=None, style='%', validate=True):
        if style != '%':
            if style == '{' or style == '$':
                raise NotImplementedError("pyc: logging format style '" + style + "' is not implemented")
            raise ValueError('Style must be one of: %, {, $')
        if fmt is None:
            fmt = '%(message)s'
        self._fmt = fmt
        self.datefmt = datefmt
        pieces = _parse_format(fmt)
        if pieces is None:
            if validate:
                raise ValueError("Invalid format '%s' for '%s' style" % (fmt, style))
            raise NotImplementedError("pyc: an unvalidated logging format is not implemented")
        for p in pieces:
            if p.key in _FRAME_FIELDS:
                raise NotImplementedError("pyc: logging format field %(" + p.key + ") is not implemented (issues/197)")
        self._pieces = pieces

    def usesTime(self):
        return self._fmt.find('%(asctime)') >= 0

    def formatTime(self, record, datefmt=None):
        if datefmt:
            return __pyc_c_call__(str, "_CG_strftime_local", str, datefmt, float, record.created)
        s = __pyc_c_call__(str, "_CG_strftime_local", str, self.default_time_format, float, record.created)
        if self.default_msec_format:
            s = self.default_msec_format % (s, record.msecs)
        return s

    def formatMessage(self, record):
        out = ''
        for p in self._pieces:
            k = p.key
            if k == '':
                out += p.text
            elif k == 'name':
                out += p.spec % record.name
            elif k == 'levelname':
                out += p.spec % record.levelname
            elif k == 'message':
                out += p.spec % record.message
            elif k == 'asctime':
                out += p.spec % record.asctime
            elif k == 'levelno':
                out += p.spec % record.levelno
            elif k == 'created':
                out += p.spec % record.created
            elif k == 'msecs':
                out += p.spec % record.msecs
            elif k == 'relativeCreated':
                out += p.spec % record.relativeCreated
            else:
                raise KeyError(k)
        return out

    def format(self, record):
        record.message = record.getMessage()
        if self.usesTime():
            record.asctime = self.formatTime(record, self.datefmt)
        return self.formatMessage(record)


_defaultFormatter = Formatter()


class Filter:
    def __init__(self, name=''):
        self.name = name
        self.nlen = len(name)

    def filter(self, record):
        if self.nlen == 0:
            return True
        elif self.name == record.name:
            return True
        elif record.name.find(self.name, 0, self.nlen) != 0:
            return False
        return record.name[self.nlen] == "."


class Filterer:
    def __init__(self):
        self.filters = []

    def addFilter(self, filter):
        if not (filter in self.filters):
            self.filters.append(filter)

    def removeFilter(self, filter):
        if filter in self.filters:
            self.filters.remove(filter)

    def filter(self, record):
        for f in self.filters:
            if not f.filter(record):
                return False
        return True


class Handler(Filterer):
    def __init__(self, level=NOTSET):
        Filterer.__init__(self)
        self.name = None
        self.level = _checkLevel(level)
        self.formatter = None

    def get_name(self):
        return self.name

    def set_name(self, name):
        self.name = name

    def setLevel(self, level):
        self.level = _checkLevel(level)

    def format(self, record):
        if self.formatter is not None:
            return self.formatter.format(record)
        return _defaultFormatter.format(record)

    def emit(self, record):
        raise NotImplementedError('emit must be implemented by Handler subclasses')

    def handle(self, record):
        rv = self.filter(record)
        if rv:
            self.emit(record)
        return rv

    def setFormatter(self, fmt):
        self.formatter = fmt

    def flush(self):
        pass

    def close(self):
        pass

    def handleError(self, record):
        if raiseExceptions:
            sys.stderr.write('--- Logging error ---\n')
            sys.stderr.write('Message: %r\nArguments: %s\n' % (record.msg, record.args))

    def __repr__(self):
        return '<%s (%s)>' % ('Handler', getLevelName(self.level))


class StreamHandler(Handler):
    terminator = '\n'

    def __init__(self, stream=None):
        Handler.__init__(self)
        if stream is None:
            self.stream = sys.stderr
        else:
            self.stream = stream

    def flush(self):
        self.stream.flush()

    def emit(self, record):
        try:
            msg = self.format(record)
            self.stream.write(msg + self.terminator)
            self.flush()
        except Exception:
            self.handleError(record)

    def setStream(self, stream):
        if stream is self.stream:
            return None
        result = self.stream
        self.flush()
        self.stream = stream
        return result


class FileHandler(StreamHandler):
    def __init__(self, filename, mode='a', encoding=None, delay=False, errors=None):
        if delay:
            raise NotImplementedError("pyc: logging.FileHandler(delay=True) is not implemented")
        self.baseFilename = filename
        self.mode = mode
        self.encoding = encoding
        StreamHandler.__init__(self, open(filename, mode))

    def close(self):
        self.stream.flush()
        self.stream.close()


class NullHandler(Handler):
    def handle(self, record):
        return True

    def emit(self, record):
        pass


lastResort = StreamHandler(sys.stderr)
lastResort.level = WARNING

_disable = 0


class Logger(Filterer):
    def __init__(self, name, level=NOTSET):
        Filterer.__init__(self)
        self.name = name
        self.level = _checkLevel(level)
        self.parent = None
        self.propagate = True
        self.handlers = []
        self.disabled = False

    def setLevel(self, level):
        self.level = _checkLevel(level)

    def debug(self, msg, *args):
        if self.isEnabledFor(DEBUG):
            self._log(DEBUG, msg, args)

    def info(self, msg, *args):
        if self.isEnabledFor(INFO):
            self._log(INFO, msg, args)

    def warning(self, msg, *args):
        if self.isEnabledFor(WARNING):
            self._log(WARNING, msg, args)

    def error(self, msg, *args):
        if self.isEnabledFor(ERROR):
            self._log(ERROR, msg, args)

    def critical(self, msg, *args):
        if self.isEnabledFor(CRITICAL):
            self._log(CRITICAL, msg, args)

    def fatal(self, msg, *args):
        if self.isEnabledFor(CRITICAL):
            self._log(CRITICAL, msg, args)

    def log(self, level, msg, *args):
        if not isinstance(level, int):
            if raiseExceptions:
                raise TypeError("level must be an integer")
            return None
        if self.isEnabledFor(level):
            self._log(level, msg, args)

    def _log(self, level, msg, args):
        record = LogRecord(self.name, level, "(unknown file)", 0, msg, args, None, "(unknown function)")
        self.handle(record)

    def handle(self, record):
        if (not self.disabled) and self.filter(record):
            self.callHandlers(record)

    def addHandler(self, hdlr):
        if not (hdlr in self.handlers):
            self.handlers.append(hdlr)

    def removeHandler(self, hdlr):
        if hdlr in self.handlers:
            self.handlers.remove(hdlr)

    def hasHandlers(self):
        c = self
        while c is not None:
            if len(c.handlers) > 0:
                return True
            if not c.propagate:
                return False
            c = c.parent
        return False

    def callHandlers(self, record):
        c = self
        found = 0
        while c is not None:
            for hdlr in c.handlers:
                found = found + 1
                if record.levelno >= hdlr.level:
                    hdlr.handle(record)
            if not c.propagate:
                c = None
            else:
                c = c.parent
        if found == 0:
            if record.levelno >= lastResort.level:
                lastResort.handle(record)

    def getEffectiveLevel(self):
        logger = self
        while logger is not None:
            if logger.level:
                return logger.level
            logger = logger.parent
        return NOTSET

    def isEnabledFor(self, level):
        if self.disabled:
            return False
        if _disable >= level:
            return False
        return level >= self.getEffectiveLevel()

    def getChild(self, suffix):
        if self is root:
            return getLogger(suffix)
        return getLogger(self.name + '.' + suffix)

    def __repr__(self):
        return '<%s %s (%s)>' % ('Logger', self.name, getLevelName(self.getEffectiveLevel()))


class RootLogger(Logger):
    def __init__(self, level):
        Logger.__init__(self, "root", level)

    def __repr__(self):
        return '<%s (%s)>' % ('RootLogger', getLevelName(self.getEffectiveLevel()))


root = RootLogger(WARNING)
_loggers = {}


def getLogger(name=None):
    if not name or name == root.name:
        return root
    if name in _loggers:
        return _loggers[name]
    rv = Logger(name)
    # The nearest existing dotted ancestor is the parent, as CPython's
    # _fixupParents finds it through its placeholders.
    rv.parent = root
    i = name.rfind('.')
    while i > 0:
        sub = name[0:i]
        if sub in _loggers:
            rv.parent = _loggers[sub]
            break
        i = name.rfind('.', 0, i - 1)
    # A logger made earlier below this one now has it as its parent, unless
    # its parent is already between the two (CPython's _fixupChildren).
    prefix = name + '.'
    for k in _loggers:
        c = _loggers[k]
        if k.startswith(prefix) and not c.parent.name.startswith(name):
            c.parent = rv
    _loggers[name] = rv
    return rv


BASIC_FORMAT = "%(levelname)s:%(name)s:%(message)s"


def basicConfig(*, filename=None, filemode='a', format=None, datefmt=None, style='%',
                level=None, stream=None, handlers=None, force=False, encoding=None, errors=None):
    if force:
        for h in root.handlers[:]:
            root.removeHandler(h)
            h.close()
    if len(root.handlers) == 0:
        if handlers is None:
            if stream is not None and filename is not None:
                raise ValueError("'stream' and 'filename' should not be specified together")
        else:
            if stream is not None or filename is not None:
                raise ValueError("'stream' or 'filename' should not be specified together with 'handlers'")
        fs = format
        if fs is None:
            fs = BASIC_FORMAT
        fmt = Formatter(fs, datefmt, style)
        if handlers is None:
            if filename is not None:
                h = FileHandler(filename, filemode)
            else:
                h = StreamHandler(stream)
            if h.formatter is None:
                h.setFormatter(fmt)
            root.addHandler(h)
        else:
            for h in handlers:
                if h.formatter is None:
                    h.setFormatter(fmt)
                root.addHandler(h)
        if level is not None:
            root.setLevel(level)


def critical(msg, *args):
    if len(root.handlers) == 0:
        basicConfig()
    root.critical(msg, *args)


def fatal(msg, *args):
    critical(msg, *args)


def error(msg, *args):
    if len(root.handlers) == 0:
        basicConfig()
    root.error(msg, *args)


def warning(msg, *args):
    if len(root.handlers) == 0:
        basicConfig()
    root.warning(msg, *args)


def info(msg, *args):
    if len(root.handlers) == 0:
        basicConfig()
    root.info(msg, *args)


def debug(msg, *args):
    if len(root.handlers) == 0:
        basicConfig()
    root.debug(msg, *args)


def log(level, msg, *args):
    if len(root.handlers) == 0:
        basicConfig()
    root.log(level, msg, *args)


def disable(level=CRITICAL):
    global _disable
    _disable = level


def shutdown():
    for h in root.handlers:
        h.flush()
    for k in _loggers:
        for h in _loggers[k].handlers:
            h.flush()
