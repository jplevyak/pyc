// Status of every module in pyc_lib/. Keep in step with the files; each
// `covers` line is taken from the module's own header and definitions.

export type Status = 'ok' | 'partial';

export interface LibModule {
  name: string;
  status: Status;
  covers: string;
  note?: string;
  /** The file under pyc_lib/, when it is not `<name>.py` (a package). */
  path?: string;
}

export const MODULES: LibModule[] = [
  { name: 'math', status: 'ok', covers: 'pi, e, tau, inf, nan; sqrt, exp/exp2/expm1, log/log1p/log2/log10, pow, trig, hyperbolic and their inverses, floor, ceil, trunc, fabs, fmod, remainder, copysign, frexp, ldexp, modf, nextafter, ulp, isnan/isinf/isfinite, isclose, gcd, lcm, factorial, comb, perm, isqrt, prod, hypot, dist, fsum, sumprod, erf/erfc, gamma/lgamma, cbrt', note: "Raises CPython's ValueError and OverflowError where CPython does (sqrt(-1), exp(1000)). fsum and gamma are CPython's own algorithms. Integer results wrap past 64 bits, as all pyc int arithmetic does." },
  { name: 'random', status: 'ok', covers: 'seed, random, getrandbits, randbytes, uniform, randrange (with step), randint, choice, choices (weights, cum_weights), shuffle, sample, triangular, gauss, normalvariate, expovariate, lognormvariate', note: 'True MT19937 with CPython’s seeding, so seeded programs match CPython draw for draw.' },
  { name: 'collections', status: 'partial', covers: 'deque (maxlen, rotate, extendleft, insert, remove, index, reverse), defaultdict, Counter (arithmetic, most_common, total, elements), OrderedDict (move_to_end, popitem(last=))', note: 'namedtuple is not implemented yet. Counter and OrderedDict are not dict subclasses, so isinstance(c, dict) is False.' },
  { name: 'itertools', status: 'ok', covers: 'count, cycle, repeat, accumulate, chain (and from_iterable), islice, compress, dropwhile, takewhile, filterfalse, starmap, groupby, zip_longest, pairwise, batched, product, permutations, combinations, combinations_with_replacement', note: 'Each consumes its input exactly as CPython’s documented equivalent does.' },
  { name: 'functools', status: 'partial', covers: 'reduce, lru_cache (with cache_info and cache_clear), cache, wraps, update_wrapper, cmp_to_key, partial, total_ordering', note: 'partial takes positional arguments; one with keyword arguments is refused at compile time (issues/189). total_ordering decorates a module-level class.' },
  { name: 'bisect', status: 'ok', covers: 'bisect_left, bisect_right, insort_left, insort_right, with key=', note: "CPython's algorithms." },
  { name: 'operator', status: 'partial', covers: 'the comparison, arithmetic, bitwise, sequence and in-place operator functions; itemgetter', note: 'attrgetter and methodcaller read an attribute named at run time, which pyc does not do.' },
  { name: 'heapq', status: 'ok', covers: 'heappush, heappop, heapreplace, heappushpop, heapify', note: "Mirrors CPython's heapq.py." },
  { name: 'copy', status: 'ok', covers: 'copy, deepcopy' },
  { name: 're', status: 'partial', covers: 'compile, search, match, fullmatch, findall, finditer, sub, subn, split, escape; Pattern and Match with their methods and attributes; named groups, backreferences, lookahead and fixed-width lookbehind, inline and scoped flags; IGNORECASE, MULTILINE, DOTALL, VERBOSE, ASCII; re.error with CPython\u2019s messages', note: 'A backtracking engine written in Python, matching CPython\u2019s results including its empty-match rules. findall with two or more groups is refused at run time (use finditer), case folding and \\w \\d \\s are ASCII, and possessive quantifiers, atomic groups and conditionals raise re.error (issues/210).' },
  { name: 'string', status: 'ok', covers: 'ascii_* constants, digits, hexdigits, octdigits, punctuation, whitespace, printable, capwords, Template (substitute, safe_substitute, get_identifiers, is_valid)' },
  { name: 'textwrap', status: 'ok', covers: 'TextWrapper, wrap, fill, shorten, dedent, indent', note: "A port of CPython 3.12's textwrap." },
  { name: 'fnmatch', status: 'ok', covers: 'fnmatch, fnmatchcase, filter', note: "CPython's fnmatch." },
  { name: 'struct', status: 'partial', covers: 'pack, unpack, unpack_from, calcsize for the integer formats' },
  { name: 'array', status: 'partial', covers: 'array with typecode; tobytes/frombytes for integer typecodes' },
  { name: 'hashlib', status: 'ok', covers: 'every algorithm in algorithms_guaranteed (md5, sha1, sha224/256/384/512, sha3_*, shake_128/256, blake2b/s with key, salt and person), new, pbkdf2_hmac, file_digest', note: 'Uses OpenSSL’s libcrypto when the system has it (loaded at run time, so its hardware-accelerated code, as CPython does), and built-in C otherwise. scrypt and the OpenSSL-only names (sha512_256, ripemd160) are not provided; file_digest takes an algorithm name.' },
  { name: 'colorsys', status: 'ok', covers: 'rgb/yiq/hls/hsv conversions', note: "Ported from CPython's colorsys.py." },
  { name: 'getopt', status: 'ok', covers: 'getopt, gnu_getopt, GetoptError', note: "Ported from CPython's getopt.py." },
  { name: 'logging', status: 'partial', covers: 'levels, getLogger and the logger hierarchy, LogRecord, Formatter (% style), Filter, StreamHandler, FileHandler, NullHandler, basicConfig, the module-level functions', note: "A port of the core of CPython's logging; output matches byte for byte. What is not ported is refused at compile time." },
  { name: 'io', status: 'partial', covers: 'StringIO, BytesIO' },
  { name: 'sys', status: 'partial', covers: 'argv, exit, maxsize, stdin/stdout/stderr, version, platform, setrecursionlimit' },
  { name: 'os', status: 'partial', path: 'os/__init__.py', covers: 'getcwd, chdir, listdir, walk, mkdir, makedirs, rmdir, removedirs, remove, rename, replace, symlink, readlink, stat, lstat, access, environ, getenv, system, getpid, cpu_count; open, read, write, close, pipe, openpty, isatty, ttyname, get_blocking, set_blocking and the O_* flags; os.path (CPython’s posixpath)', note: "POSIX only. Failures raise CPython's OSError subclass and message." },
  { name: 'pathlib', status: 'ok', covers: 'PurePath and Path: parsing, parts, suffixes, joinpath, relative_to, match, stat, exists/is_dir/is_file, read/write text and bytes, iterdir, walk, glob, rglob, resolve, mkdir, touch, unlink, rename', note: "A port of CPython 3.12's pathlib for its POSIX flavour." },
  { name: 'time', status: 'partial', covers: 'time, time_ns, sleep, perf_counter, monotonic, process_time and their _ns forms, struct_time, gmtime, localtime, mktime, strftime, asctime, ctime' },
  { name: 'dataclasses', status: 'partial', covers: '@dataclass and field(...)', note: 'The frontend generates the class CPython’s decorator builds. fields, asdict, astuple and replace are refused at import.' },
  { name: 'typing', status: 'ok', covers: 'The names programs import: Any, Optional, Union, Callable, List, Dict, Tuple, Iterable, TypeVar, cast, overload, ...', note: 'Annotations are accepted and ignored; inference supplies the types.' },
  { name: 'enum', status: 'partial', covers: 'Enum, auto, unique; members by name, by value and by iteration (Color(1), Color["RED"], for m in Color, len, in), aliases, _missing_, __init__ on member values', note: 'IntEnum and StrEnum (an int or str subclass carrying attributes), Flag/IntFlag and the functional API are refused (issues/208).' },
  { name: 'ctypes', status: 'partial', path: 'ctypes/__init__.py', covers: 'CDLL, argtypes, restype, the c_* types, Structure, POINTER, byref, pointer, sizeof', note: 'Lowered at compile time to direct C calls. A use outside the subset is refused with its line.' },
  { name: 'socket', status: 'partial', covers: 'socket (bind, listen, accept, recv, send, setsockopt), gethostname' },
  { name: 'select', status: 'partial', covers: 'select', note: "CPython's results, timeouts and errors, over poll(2). poll, epoll and kqueue are not provided." },
  { name: 'termios', status: 'partial', covers: 'tcgetattr, tcsetattr, tcsendbreak, tcdrain, tcflush, tcflow, tcgetwinsize, tcsetwinsize, and the constants Linux and macOS share', note: 'tcgetattr returns its cc list as ints, where CPython mixes 1-byte bytes and ints: a permissive accommodation (PYC_TERMIOS_CC_INT) that warns at each call and is refused under --strict.' },
  { name: 'fcntl', status: 'partial', covers: 'fcntl, ioctl (integer and buffer forms), flock, F_* and LOCK_* constants', note: 'A mutable-buffer (bytearray) argument raises NotImplementedError; lockf is not provided.' },
  { name: 'serial', status: 'partial', path: 'serial/__init__.py', covers: 'A port of pyserial 3.5 for POSIX: Serial with its settings, read/write with timeouts, readline/readlines/read_until/iteration, modem lines, break, flush, cancel_read/cancel_write, exclusive access', note: 'Output matches pyserial’s over a pseudo-terminal. A port whose timeout changes between None and a number is refused at compile time. serial_for_url, serial.tools and rs485 are not provided.' },
];

export interface Wanted {
  name: string;
  why: string;
}

// The next modules to write, ordered roughly by how often real programs import them.
export const WANTED: Wanted[] = [
  { name: 'enum (complete)', why: 'IntEnum, StrEnum and Flag.' },
  { name: 'collections.namedtuple', why: 'A tuple subclass with named fields; typing.NamedTuple with it.' },
  { name: 'functools.partial keywords', why: 'partial(f, key=value), which needs ** into named parameters.' },
  { name: 'json', why: 'Serialization of typed records, and parsing into a declared shape.' },
  { name: 'asyncio', why: 'A CPython-compatible front end for the event loop async/await already compiles to.' },
  { name: 'statistics, fractions, decimal', why: 'Numeric work beyond math.' },
  { name: 'unittest', why: 'Test the compiled program with the same tests that run under CPython.' },
];
