import type { Metadata } from 'next';
import Link from 'next/link';
import { Code } from '@/components/Code';
import { Callout, DocLayout, PageHero } from '@/components/DocPage';
import { repo } from '@/content/site';

export const metadata: Metadata = {
  title: 'Tour of pyc',
  description:
    'A guided tour of pyc: type inference without annotations, classes, generators, exceptions, pattern matching, the standard library, and how the compiler works.',
};

const TOC = [
  { id: 'model', label: 'The model' },
  { id: 'inference', label: 'Inference, not annotation' },
  { id: 'classes', label: 'Classes and operators' },
  { id: 'generators', label: 'Generators' },
  { id: 'exceptions', label: 'Exceptions' },
  { id: 'match', label: 'Pattern matching' },
  { id: 'collections', label: 'Collections and strings' },
  { id: 'stdlib', label: 'The standard library' },
  { id: 'refusals', label: 'What pyc refuses' },
  { id: 'how', label: 'How it works' },
];

export default function Learn() {
  return (
    <>
      <PageHero title="A tour of pyc">
        Every example on this page compiles with <code>pyc --strict</code>, and its output is
        byte-for-byte what CPython prints.
      </PageHero>
      <DocLayout toc={TOC}>
        <h2 id="model">The model</h2>
        <p>
          pyc is a <strong>whole-program</strong> compiler. It reads your program and everything
          it imports, and works out the set of concrete types that can reach every variable,
          field, parameter and container element. Then it compiles one native version of each
          function and class for each distinct combination of types it found.
        </p>
        <p>The rules follow from that:</p>
        <ul>
          <li>
            Code is not loaded at run time. <code>eval</code>, <code>exec</code> and dynamic{' '}
            <code>import</code> are out of scope by design.
          </li>
          <li>
            A value must have a representation. An <code>int</code> is a 64-bit machine word, a
            class instance is a struct, a <code>list</code> of floats is an array of doubles.
            A slot that genuinely holds two unrelated types, such as <code>int</code> in one
            branch and <code>str</code> in the other, has no representation, and pyc refuses it.
          </li>
          <li>
            Inside those limits the semantics are CPython&apos;s. The test suite checks every
            program against CPython&apos;s output.
          </li>
        </ul>

        <h2 id="inference">Inference, not annotation</h2>
        <p>
          You write no types. The same class can hold different types in different objects.
          pyc gives each one its own layout, and calls the right specialized method directly.
        </p>
        <Code title="box.py">{`
class Box:
    def __init__(self, value):
        self.value = value

    def describe(self):
        return f"Box({self.value!r})"

print(Box(42).describe())
print(Box("hello").describe())
print(Box([1.5, 2.5]).describe())
`}</Code>
        <Code lang="shell">{`
$ pyc --strict box.py && ./box
Box(42)
Box('hello')
Box([1.5, 2.5])
`}</Code>
        <p>
          The analysis behind this is <strong>demand-driven</strong>. It starts with one
          version of every function and one of every class, and splits a version only when
          something needs the split: an operation that can&apos;t proceed on a union of types, a
          dispatch that can&apos;t be resolved statically. Here an <code>int</code>, a{' '}
          <code>str</code> and a <code>list</code> have no common representation, so{' '}
          <code>Box</code> gets one layout for each.
        </p>

        <h2 id="classes">Classes and operators</h2>
        <p>
          Inheritance, <code>super()</code>, class attributes, dunder methods and read-only{' '}
          <code>@property</code> work as in CPython. An operator on objects compiles to a direct
          call to the right method.
        </p>
        <Code title="vec.py">{`
class Vec:
    def __init__(self, x, y):
        self.x, self.y = x, y

    def __add__(self, other):
        return Vec(self.x + other.x, self.y + other.y)

    def __mul__(self, k):
        return Vec(self.x * k, self.y * k)

    def __eq__(self, other):
        return self.x == other.x and self.y == other.y

    def __repr__(self):
        return f"Vec({self.x}, {self.y})"

v = Vec(1, 2) + Vec(3, 4) * 2
print(v, v == Vec(7, 10))       # Vec(7, 10) True
`}</Code>

        <h2 id="generators">Generators</h2>
        <p>
          <code>yield</code> and <code>yield from</code> compile to native coroutines. An
          infinite generator is fine, because nothing is materialized until you ask for it.
        </p>
        <Code title="gen.py">{`
def fib():
    a, b = 0, 1
    while True:
        yield a
        a, b = b, a + b

def take(n, gen):
    out = []
    for x in gen:
        if len(out) == n:
            break
        out.append(x)
    return out

def evens(xs):
    for x in xs:
        if x % 2 == 0:
            yield x

def chain(a, b):
    yield from a
    yield from b

print(take(10, fib()))      # [0, 1, 1, 2, 3, 5, 8, 13, 21, 34]
print(list(chain(evens(range(10)), evens(range(20, 26)))))
                            # [0, 2, 4, 6, 8, 20, 22, 24]
`}</Code>

        <h2 id="exceptions">Exceptions</h2>
        <p>
          <code>try</code>/<code>except</code>/<code>else</code>/<code>finally</code>, user
          exception hierarchies, and CPython&apos;s built-in exceptions raised by the runtime
          itself: <code>ValueError</code> from <code>int(&quot;x&quot;)</code>,{' '}
          <code>KeyError</code>, <code>StopIteration</code> and the rest.
        </p>
        <Code title="parse.py">{`
class ParseError(Exception):
    pass

def parse(text):
    try:
        return int(text)
    except ValueError:
        raise ParseError("not a number: " + text)

for s in ["12", "x7", "40"]:
    try:
        print(parse(s) * 2)
    except ParseError as e:
        print("error:", e)
    finally:
        print("checked", s)
`}</Code>

        <h2 id="match">Pattern matching</h2>
        <p>
          <code>match</code>/<code>case</code> supports every PEP 634 pattern kind: literal,
          capture, wildcard, sequence, mapping, class, OR and guard.
        </p>
        <Code title="where.py">{`
def describe(point):
    match point:
        case (0, 0):
            return "origin"
        case (0, y):
            return f"on the y axis at {y}"
        case (x, 0):
            return f"on the x axis at {x}"
        case (x, y) if x == y:
            return "on the diagonal"
        case _:
            return "somewhere else"

for p in [(0, 0), (0, 3), (2, 0), (4, 4), (1, 2)]:
    print(p, describe(p))
`}</Code>

        <h2 id="collections">Collections and strings</h2>
        <p>
          <code>list</code>, <code>tuple</code>, <code>dict</code>, <code>set</code>,{' '}
          <code>str</code>, <code>bytes</code> and <code>bytearray</code> each compile to a
          specialized, element-typed native structure. Comprehensions, generator expressions,
          slicing, f-strings and <code>%</code>-formatting all work. Dicts keep insertion order,
          as in CPython.
        </p>
        <Code title="words.py">{`
words = "the quick brown fox jumps over the lazy dog the end".split()
freq = {}
for w in words:
    freq[w] = freq.get(w, 0) + 1

top = sorted(freq.items(), key=lambda kv: (-kv[1], kv[0]))[:3]
print(top)                      # [('the', 3), ('brown', 1), ('dog', 1)]
print({w: len(w) for w in words if len(w) > 4})
print(", ".join(w.upper() for w in words[:3]))
`}</Code>

        <h2 id="stdlib">The standard library</h2>
        <p>
          Imports resolve at compile time. The modules in <code>pyc_lib/</code> are written in
          Python and compiled with your program, so they cost nothing you don&apos;t use.{' '}
          <code>random</code> is the real Mersenne Twister, so a seeded program draws the same
          numbers CPython does:
        </p>
        <Code title="dice.py">{`
import random
from collections import defaultdict

random.seed(2026)
counts = defaultdict(int)
for _ in range(1000):
    counts[random.randint(1, 6)] += 1
print(sorted(counts.items()))
print(random.random())          # 0.7894507294053692, as in CPython
`}</Code>
        <p>
          See <Link href="/libraries/">Libraries</Link> for what each module covers.
        </p>

        <h2 id="refusals">What pyc refuses</h2>
        <p>
          When a program needs a value that no static type can represent, pyc says so at
          compile time, names the expression, and shows the call that led there:
        </p>
        <Code title="pick.py">{`
import sys

def pick(flag):
    if flag:
        return 1
    return "one"

print(pick(len(sys.argv) > 1))
`}</Code>
        <Code lang="shell">{`
$ pyc pick.py
!err pick.py:3:9: error: expression has mixed basic types:( int64 str )
!err     def pick(flag):
!err             ^
!err   called from pick.py:8
!dim ...
!err fail: program does not type
`}</Code>
        <p>
          Some unions are only an artifact of how a program is written. For a few of them,
          such as a numeric slot that holds both <code>int</code> and <code>float</code>, the
          default <code>--permissive</code> mode applies a <em>named accommodation</em> and warns
          at the line. <code>--strict</code> turns every accommodation off.
        </p>
        <Callout title="The contract.">
          <p>
            Under <code>--strict</code>, a program has CPython&apos;s semantics or it does not
            compile. Any gap in that guarantee is a bug. See{' '}
            <Link href="/strict/">the strict dialect</Link>.
          </p>
        </Callout>

        <h2 id="how">How it works</h2>
        <Code lang="text" title="pipeline">{`
Python source (.py)
  → DParser (python.g)      parse to an AST; no CPython involved
  → build_syms              symbols, scopes, classes
  → build_if1               lower to IF1, the analysis IR
  → IFA flow analysis       type inference with demand-driven splitting
  → clone                   one concrete function / type per contour
  → optimize                dead code, inlining, constant folding
  → C codegen → cc          or LLVM codegen → binary / JIT
`}</Code>
        <p>
          The core is <strong>IFA</strong>, a simultaneous data- and control-flow analysis by
          abstract interpretation over a type-value lattice. It analyses <em>contours</em>:
          versions of functions (EntrySets) and versions of data types (CreationSets). It starts
          from the minimum number of each and splits one only on demand, never because a
          neighbouring contour split. Every pass re-derives the types from scratch and keeps
          only the decisions, so a merge made early can be taken back later.
        </p>
        <p>
          IFA is a standalone library under <code>ifa/</code>. pyc drives it through a callback
          interface that keeps everything Python-specific out of the analysis. The repository
          documents it in depth: start with{' '}
          <a href={repo('blob/main/PIPELINE.md')}>PIPELINE.md</a> and{' '}
          <a href={repo('blob/main/ifa/INDEX.md')}>ifa/INDEX.md</a>.
        </p>
      </DocLayout>
    </>
  );
}
