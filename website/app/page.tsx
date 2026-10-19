import Link from 'next/link';
import { Code } from '@/components/Code';
import { Benchmarks } from '@/components/Benchmarks';
import { FACTS, repo } from '@/content/site';

const HERO_SRC = `
class Shape:
    def area(self):
        return 0.0

class Circle(Shape):
    def __init__(self, r):
        self.r = r
    def area(self):
        return 3.14159 * self.r * self.r

class Square(Shape):
    def __init__(self, s):
        self.s = s
    def area(self):
        return self.s * self.s

shapes = [Circle(1.5), Square(2.0), Circle(0.5)]
print(sum((s.area() for s in shapes), 0.0))
`;

const HERO_SHELL = `
$ pyc --strict shapes.py
$ ./shapes
11.853974999999998
$ python3 shapes.py
11.853974999999998
`;

const FEATURES = [
  {
    icon: '∅',
    title: 'No annotations',
    body: 'Write ordinary Python. pyc infers a type for every value in the whole program, then compiles a specialized version of each function and class for the types that actually reach it.',
  },
  {
    icon: '⚙',
    title: 'Native binaries',
    body: 'The output is a stand-alone executable built from C or LLVM IR. There is no interpreter and no libpython at run time, and no CPython at compile time either.',
  },
  {
    icon: '=',
    title: 'CPython semantics',
    body: 'A compiled program prints what CPython prints, or pyc refuses it at compile time. It never deviates silently, and the end-to-end suite checks every test against CPython.',
  },
  {
    icon: '⑂',
    title: 'Two backends',
    body: 'The C backend is the default. The LLVM backend (-b) emits IR directly and also drives a JIT and an interactive REPL. CI runs the full suite on both.',
  },
  {
    icon: '◇',
    title: 'Demand-driven precision',
    body: 'The analysis starts from the fewest contours possible and splits a function or a data type only when a real type distinction demands it. Code size tracks the program, not the call graph.',
  },
  {
    icon: '⌁',
    title: 'Real Python',
    body: 'Classes and inheritance, closures, exceptions, generators, async/await, match/case, comprehensions, f-strings, packages and relative imports.',
  },
];

export default function Home() {
  return (
    <>
      <section className="hero">
        <div className="wrap hero-grid">
          <div>
            <span className="eyebrow">Python 3 → native code</span>
            <h1>
              Python, <span className="accent">compiled</span>.
            </h1>
            <p className="lede">
              pyc is a whole-program compiler for Python 3. It infers every type with no
              annotations and emits a native binary that behaves exactly as CPython does, or it
              tells you at compile time why it can&apos;t.
            </p>
            <div className="cta">
              <Link className="btn primary" href="/install/">
                Install pyc
              </Link>
              <Link className="btn" href="/learn/">
                Take the tour
              </Link>
              <a className="btn" href={repo('')}>
                Source on GitHub
              </a>
            </div>
          </div>
          <div className="stack">
            <Code title="shapes.py">{HERO_SRC}</Code>
            <Code lang="shell" title="terminal">
              {HERO_SHELL}
            </Code>
          </div>
        </div>
      </section>

      <section className="section">
        <div className="wrap stats">
          <div className="stat">
            <div className="num">
              {FACTS.corpusCompile}/{FACTS.corpusTotal}
            </div>
            <div className="label">shedskin benchmark programs compile and run</div>
          </div>
          <div className="stat">
            <div className="num">{FACTS.corpusMatch}</div>
            <div className="label">print output byte-identical to CPython; 17 more can&apos;t be compared because CPython hits the time cap</div>
          </div>
          <div className="stat">
            <div className="num">{FACTS.e2ePass}</div>
            <div className="label">end-to-end tests, checked against CPython on both backends</div>
          </div>
          <div className="stat">
            <div className="num">0</div>
            <div className="label">type annotations required</div>
          </div>
        </div>
      </section>

      <section className="section alt">
        <div className="wrap">
          <div className="section-head">
            <h2>Why pyc</h2>
            <p>
              pyc treats the whole program as one unit. Because it sees every call and every
              allocation, it can compile dynamic-looking Python into monomorphic native code.
            </p>
          </div>
          <div className="grid-3">
            {FEATURES.map((f) => (
              <div className="card" key={f.title}>
                <div className="icon" aria-hidden="true">
                  {f.icon}
                </div>
                <h3>{f.title}</h3>
                <p>{f.body}</p>
              </div>
            ))}
          </div>
        </div>
      </section>

      <section className="section">
        <div className="wrap grid-2">
          <div>
            <h2 style={{ marginTop: 0 }}>Never silently different</h2>
            <p>
              Some Python can&apos;t be typed statically without boxing. pyc never quietly papers
              over that. By default it applies a few <em>named</em> accommodations, each of
              which warns at the line it affects. Under <code>--strict</code> the program has
              CPython&apos;s semantics or it doesn&apos;t compile.
            </p>
            <p>
              In this example the analysis also finds that <code>d</code> only ever holds
              integers. It gives <code>d</code> its own integer-typed layout of{' '}
              <code>Counter</code>, so <code>d.total</code> prints <code>1</code> exactly as
              CPython does.
            </p>
            <p>
              <Link href="/strict/">The strict dialect →</Link>
            </p>
          </div>
          <div className="stack">
            <Code title="counter.py">{`
class Counter:
    def __init__(self):
        self.total = 0
    def add(self, v):
        self.total = self.total + v

c = Counter()
c.add(1)
c.add(2.5)
print(c.total)      # 3.5
d = Counter()
d.add(1)
print(d.total)      # 1
`}</Code>
            <Code lang="shell" title="terminal">{`
$ pyc counter.py
!warn counter.py:3: warning: 'total' holds both int and float; widened to
!warn float64, so an int value in it prints as a float (CPython keeps the int).
!dim ...
!warn Permissive numeric coercion, refused under --strict (issues/171)
$ ./counter
3.5
1
$ pyc --strict counter.py
!err counter.py:5:69: error: expression has mixed basic types:( int64 float64 )
!dim ...
!err fail: program does not type
`}</Code>
          </div>
        </div>
      </section>

      <section className="section alt">
        <div className="wrap">
          <div className="section-head">
            <h2>Fast because it&apos;s typed</h2>
            <p>
              Once every value has a concrete type, an attribute read becomes a field load, a
              method call becomes a direct call, and an <code>int</code> is a machine word.
              These are wall-clock times for unmodified benchmark programs.
            </p>
          </div>
          <Benchmarks />
        </div>
      </section>

      <section className="section">
        <div className="wrap">
          <div className="section-head">
            <h2>Where pyc is going</h2>
            <p>
              The compiler core is solid on a 77-program benchmark corpus. The next phase is
              breadth and definition: a much larger standard library, and a strict mode
              developed into a fully specified dialect of Python.
            </p>
          </div>
          <div className="grid-3">
            <div className="card">
              <h3>More libraries</h3>
              <p>
                {FACTS.libModules} standard-library modules ship today, from <code>math</code>{' '}
                and <code>random</code> (true MT19937) to <code>re</code>, <code>struct</code>{' '}
                and <code>heapq</code>. Each one is real or it raises. None returns made-up values.
              </p>
              <p>
                <Link href="/libraries/">Library status →</Link>
              </p>
            </div>
            <div className="card">
              <h3>A strict dialect</h3>
              <p>
                Strict pyc is already &ldquo;CPython or a compile error.&rdquo; The goal is to make
                it a language in its own right, with a specification, precise diagnostics and a
                typed standard library.
              </p>
              <p>
                <Link href="/strict/">Read the plan →</Link>
              </p>
            </div>
            <div className="card">
              <h3>The roadmap</h3>
              <p>
                <code>**kwargs</code>, property setters, <code>complex</code>, a wider{' '}
                <code>re</code>, packaging, and the tooling that turns a compiler into a
                language.
              </p>
              <p>
                <Link href="/roadmap/">See the roadmap →</Link>
              </p>
            </div>
          </div>
        </div>
      </section>
    </>
  );
}
