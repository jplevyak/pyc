import type { Metadata } from 'next';
import Link from 'next/link';
import { Code } from '@/components/Code';
import { Callout, DocLayout, PageHero } from '@/components/DocPage';
import { repo } from '@/content/site';

export const metadata: Metadata = {
  title: 'Strict dialect',
  description:
    'Strict pyc: a dialect of Python 3 defined by one guarantee, CPython semantics or a compile error. What it is today and how it becomes a fully specified language.',
};

const TOC = [
  { id: 'contract', label: 'The contract' },
  { id: 'restriction', label: 'A dialect by restriction' },
  { id: 'today', label: 'Strict today' },
  { id: 'accommodations', label: 'Accommodations' },
  { id: 'dialect', label: 'Toward a full dialect' },
  { id: 'never', label: 'What it will not do' },
];

const PILLARS: { title: string; state: 'now' | 'next'; body: React.ReactNode }[] = [
  {
    title: 'A written specification',
    state: 'next',
    body: (
      <>
        Today, the strict language is whatever the analysis accepts. A dialect needs rules a
        programmer can read: which unions are representable, what <code>int</code> means, how{' '}
        <code>None</code> combines with other types, when a container element is
        &ldquo;one type.&rdquo; The rules should be stated in terms of what the types{' '}
        <em>are</em>, never in terms of how the analysis happens to find them.
      </>
    ),
  },
  {
    title: 'Stability',
    state: 'next',
    body: (
      <>
        A program that version <em>N</em> accepts under <code>--strict</code>, version{' '}
        <em>N+1</em> accepts too, with the same output. The shedskin corpus and the end-to-end
        suite become a conformance suite, run against every release and against CPython.
      </>
    ),
  },
  {
    title: 'Diagnostics as the interface',
    state: 'now',
    body: (
      <>
        In a strict dialect, the refusal message <em>is</em> the type system&apos;s user
        interface. pyc already names both sources of a numeric union (&ldquo;the int comes from
        line 3, the float from line 5&rdquo;). Every refusal should point at where the values
        meet, cite the spec rule, and say what change would make the program typeable.
      </>
    ),
  },
  {
    title: 'Checked annotations',
    state: 'next',
    body: (
      <>
        Annotations parse today and, as in CPython, have no effect. Under{' '}
        <code>--strict</code> they will be verified against what inference found. They stay
        optional, since inference still does the work, but they let a module state its
        interface and turn a confusing downstream error into a clear one at the boundary.
      </>
    ),
  },
  {
    title: 'A strict standard library',
    state: 'next',
    body: (
      <>
        Every module in <code>pyc_lib/</code> compiles under <code>--strict</code> with no
        accommodation, and its surface is documented in dialect terms. A strict program can
        then use the whole library with the same guarantee as its own code.
      </>
    ),
  },
  {
    title: 'Integers, decided',
    state: 'next',
    body: (
      <>
        <code>int</code> is 64 bits. Literals that don&apos;t fit are already refused under{' '}
        <code>--strict</code>. Arithmetic that overflows at run time is the remaining gap, and
        the dialect has to close it explicitly: either trap on overflow, or use a wider
        representation where the program demands one. Wrapping silently is not an option.
      </>
    ),
  },
  {
    title: 'Tooling',
    state: 'next',
    body: (
      <>
        The compiler already knows the type of every expression, and <code>--html</code>{' '}
        renders the type-annotated program. Those facts belong in the editor: a language server
        with inferred types on hover, refusals as live diagnostics, and go-to-definition that
        follows the resolved call graph.
      </>
    ),
  },
];

export default function Strict() {
  return (
    <>
      <PageHero title="The strict dialect">
        <code>pyc --strict</code> compiles a dialect of Python 3 defined by a single guarantee: your
        program has CPython&apos;s semantics, or it does not compile.
      </PageHero>
      <DocLayout toc={TOC}>
        <h2 id="contract">The contract</h2>
        <p>
          Static compilation of Python always runs into programs that CPython can run and that
          no static type assignment can represent without boxing every value. Compilers usually
          handle these programs in one of three ways: box everything and lose the speed, deviate
          quietly from CPython, or ask for annotations. Strict pyc does none of these:
        </p>
        <Callout title="Strict mode:">
          <p>
            every accepted program prints exactly what CPython prints. Every program that
            can&apos;t meet that is refused at compile time, with a diagnostic. A gap in this
            guarantee is a bug, tracked as a silent wrong answer.
          </p>
        </Callout>

        <h2 id="restriction">A dialect by restriction</h2>
        <p>
          Strict pyc adds no syntax. Every strict program is an ordinary Python 3 program that
          runs unchanged under CPython, so you can develop and test with CPython and ship a pyc
          binary. The dialect is the subset of Python 3 whose programs are{' '}
          <em>monotonic</em>: every variable, field and container element can be given one
          concrete representation for the life of the program.
        </p>
        <p>
          That is a larger subset than it sounds. Polymorphism is fine, as long as each object
          is consistent. A function called with ints and with strings gets two compiled
          versions, a class used with several field types gets several layouts, and related
          classes dispatch through a shared base. The analysis finds all of this without
          annotations. What&apos;s excluded is a <em>single</em> slot holding unrelated types,
          and the features that defeat whole-program analysis (<code>eval</code>,{' '}
          <code>exec</code>, run-time <code>setattr</code>).
        </p>

        <h2 id="today">Strict today</h2>
        <ul>
          <li>
            Every type violation is fatal, and no accommodation applies.
          </li>
          <li>
            The end-to-end suite is compiled under <code>--strict</code> and compared with
            CPython. A strict corpus sweep checks the 77 shedskin programs the same way. Every
            program must match CPython or be refused.
          </li>
          <li>
            Integer literals past 64 bits are refused rather than wrapped.
          </li>
          <li>
            Python 2 idioms that CPython 3 rejects, such as <code>raise &quot;message&quot;</code>,
            are refused.
          </li>
        </ul>
        <Code title="counter.py">{`
class Counter:
    def __init__(self):
        self.total = 0
    def add(self, v):
        self.total = self.total + v

c = Counter()
c.add(1)
c.add(2.5)
print(c.total)
`}</Code>
        <Code lang="shell">{`
$ pyc --strict counter.py
!err counter.py:5:69: error: expression has mixed basic types:( int64 float64 )
!err             self.total = self.total + v
!err                                        ^
!err   called from counter.py:8
!dim ...
!err fail: program does not type
`}</Code>
        <p>
          The fix is the one that makes your intent explicit, and it changes nothing under
          CPython: <code>self.total = 0.0</code>.
        </p>

        <h2 id="accommodations">Accommodations</h2>
        <p>
          The default mode, <code>--permissive</code>, exists to compile more existing code. It
          allows a short list of <strong>accommodations</strong>: behaviour that goes beyond
          CPython 3 or beyond static typing in order to accept a program. Every accommodation
          follows the same three rules:
        </p>
        <ol>
          <li>It sits behind a named flag, on under permissive and off under strict.</li>
          <li>It warns wherever it changes what the program would do under CPython.</li>
          <li>
            It reaches the generic analysis only through a callback interface. The analysis
            library itself knows nothing about Python.
          </li>
        </ol>
        <p>
          Examples: widening an <code>int</code>/<code>float</code> slot to <code>float</code>,
          and wrapping a Python 2 string <code>raise</code> in an <code>Exception</code>. The
          complete audit is{' '}
          <a href={repo('blob/master/issues/closed/171-permissive-accommodations-must-be-flagged-and-non-strict.md')}>
            issues/171
          </a>
          .
        </p>

        <h2 id="dialect">Toward a full dialect</h2>
        <p>
          The guarantee is in place. What makes strict pyc a <em>language</em>, rather than a
          compiler mode, is everything around it: a definition you can read, a promise that it
          won&apos;t move under you, and tools that speak it.
        </p>
        <div className="grid-2">
          {PILLARS.map((p) => (
            <div className="card" key={p.title}>
              <h3>
                {p.title}{' '}
                <span className={`pill ${p.state === 'now' ? 'partial' : 'planned'}`}>
                  {p.state === 'now' ? 'under way' : 'planned'}
                </span>
              </h3>
              <p>{p.body}</p>
            </div>
          ))}
        </div>
        <p style={{ marginTop: '1.4em' }}>
          The sequencing is on the <Link href="/roadmap/">roadmap</Link>.
        </p>

        <h2 id="never">What it will not do</h2>
        <ul>
          <li>
            <strong>Box.</strong> A slot that holds unrelated types has no representation in the
            dialect. Boxing would bring back the cost static compilation exists to remove, and
            it would hide the very unions the dialect is meant to surface.
          </li>
          <li>
            <strong>Deviate to type a program.</strong> If a change makes pyc agree with some
            other compiler and disagree with CPython, it&apos;s wrong. CPython is the reference.
          </li>
          <li>
            <strong>Add syntax.</strong> Strict programs stay valid Python, so CPython remains
            your interpreter, debugger and test runner.
          </li>
        </ul>
      </DocLayout>
    </>
  );
}
