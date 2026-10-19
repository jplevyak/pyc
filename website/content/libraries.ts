// Status of every module in pyc_lib/. Keep in step with the files; each
// `covers` line is taken from the module's own header and definitions.

export type Status = 'ok' | 'partial';

export interface LibModule {
  name: string;
  status: Status;
  covers: string;
  note?: string;
}

export const MODULES: LibModule[] = [
  { name: 'math', status: 'partial', covers: 'pi, e, tau, inf; sqrt, exp, log/log2/log10, pow, hypot, fabs, fmod, floor, ceil, trig and hyperbolic functions, radians, degrees', note: "Bound straight to libc, so results match CPython's C-backed math." },
  { name: 'random', status: 'ok', covers: 'seed, random, getrandbits, uniform, randrange, randint, choice, shuffle, sample, triangular', note: 'True MT19937 with CPython’s seeding, so seeded programs match CPython draw for draw.' },
  { name: 'collections', status: 'partial', covers: 'deque, defaultdict' },
  { name: 'heapq', status: 'ok', covers: 'heappush, heappop, heapreplace, heappushpop, heapify', note: "Mirrors CPython's heapq.py." },
  { name: 'itertools', status: 'partial', covers: 'count, product' },
  { name: 'functools', status: 'partial', covers: 'reduce' },
  { name: 'copy', status: 'ok', covers: 'copy, deepcopy' },
  { name: 're', status: 'partial', covers: 'compile, match, fullmatch, search; classes, groups, alternation, greedy and lazy quantifiers, anchors', note: 'A backtracking engine written in Python. findall, sub and split are not implemented yet.' },
  { name: 'fnmatch', status: 'ok', covers: 'fnmatch, fnmatchcase, filter' },
  { name: 'string', status: 'ok', covers: 'ascii_* constants, digits, punctuation, whitespace, printable, capwords' },
  { name: 'struct', status: 'partial', covers: 'pack, unpack, unpack_from, calcsize for the integer formats' },
  { name: 'array', status: 'partial', covers: 'array with typecode; tobytes/frombytes for integer typecodes' },
  { name: 'hashlib', status: 'partial', covers: 'md5 (RFC 1321)', note: 'The other algorithms raise NotImplementedError.' },
  { name: 'colorsys', status: 'ok', covers: 'rgb/yiq/hls/hsv conversions', note: "Ported from CPython's colorsys.py." },
  { name: 'getopt', status: 'ok', covers: 'getopt, gnu_getopt, GetoptError', note: "Ported from CPython's getopt.py." },
  { name: 'io', status: 'partial', covers: 'StringIO, BytesIO' },
  { name: 'sys', status: 'partial', covers: 'argv, exit, maxsize, stdin/stdout/stderr, version, platform, setrecursionlimit' },
  { name: 'os', status: 'partial', covers: 'listdir, walk, getcwd, chdir, mkdir, remove, rename, stat, system, path' },
  { name: 'time', status: 'partial', covers: 'time, sleep' },
  { name: 'socket', status: 'partial', covers: 'socket (bind, listen, accept, recv, send, setsockopt), gethostname' },
  { name: 'select', status: 'partial', covers: 'select' },
  { name: 'serial', status: 'partial', covers: 'The pyserial API surface', note: 'Opening a port raises SerialException, as pyserial does for a port it cannot open.' },
];

export interface Wanted {
  name: string;
  why: string;
}

// The next modules to write, ordered roughly by how often real programs import them.
export const WANTED: Wanted[] = [
  { name: 'dataclasses', why: 'The idiomatic record type. A natural fit: fields and their types are fixed per class.' },
  { name: 'enum', why: 'Enumerations compile to integers with names, with no run-time cost.' },
  { name: 'typing', why: 'Accept annotations, and in strict mode check them against what inference found.' },
  { name: 'itertools (complete)', why: 'chain, islice, permutations, combinations, groupby, accumulate, zip_longest.' },
  { name: 'functools (complete)', why: 'partial, lru_cache, cache, cmp_to_key, total_ordering.' },
  { name: 'collections (complete)', why: 'Counter, OrderedDict, namedtuple.' },
  { name: 're (complete)', why: 'findall, finditer, sub, split, named groups, flags.' },
  { name: 'bisect', why: 'Small, pure Python, widely used.' },
  { name: 'json', why: 'Serialization of typed records, and parsing into a declared shape.' },
  { name: 'asyncio', why: 'A CPython-compatible front end for the event loop async/await already compiles to.' },
  { name: 'pathlib / os.path', why: 'Modern file-system access.' },
  { name: 'statistics, fractions, decimal', why: 'Numeric work beyond math.' },
  { name: 'hashlib (complete)', why: 'sha1 and the sha2 family.' },
  { name: 'unittest', why: 'Test the compiled program with the same tests that run under CPython.' },
];
