import type { Metadata } from 'next';
import { Code } from '@/components/Code';
import { Callout, DocLayout, PageHero } from '@/components/DocPage';
import { MODULES, WANTED } from '@/content/libraries';
import { repo } from '@/content/site';

export const metadata: Metadata = {
  title: 'Libraries',
  description:
    "The Python standard-library modules pyc ships today, what each covers, and what's next.",
};

const TOC = [
  { id: 'how', label: 'How libraries work' },
  { id: 'rule', label: 'Real or raise' },
  { id: 'modules', label: 'Modules today' },
  { id: 'next', label: 'Next up' },
  { id: 'write', label: 'Writing a module' },
];

export default function Libraries() {
  return (
    <>
      <PageHero title="Libraries">
        pyc&apos;s standard library is ordinary Python, compiled with your program. Growing it
        is the most direct way to widen what pyc can compile.
      </PageHero>
      <DocLayout toc={TOC}>
        <h2 id="how">How libraries work</h2>
        <p>
          There is no prebuilt runtime library to link against. <code>import math</code>{' '}
          resolves at compile time to <code>pyc_lib/math.py</code>, which is analysed and
          compiled along with the rest of the program. A function you never call is never
          emitted, and a library function is specialized for the types your program passes it,
          exactly like your own code.
        </p>
        <p>
          The built-in types themselves (<code>str</code>, <code>list</code>, <code>dict</code>,{' '}
          <code>set</code>, exceptions, generators) are also Python, in <code>__pyc__/</code>,
          on top of a small C runtime.
        </p>

        <h2 id="rule">Real or raise</h2>
        <Callout title="The rule for every module:">
          <p>
            a function either computes CPython&apos;s answer, or it raises the exception CPython
            would raise for something it cannot do. A stub that returns a plausible made-up
            value is a silent wrong answer, and silent wrong answers are bugs.
          </p>
        </Callout>
        <p>
          A name that doesn&apos;t exist yet is a compile error, never a wrong result.
        </p>

        <h2 id="modules">Modules today</h2>
        <div className="table-wrap">
          <table>
            <thead>
              <tr>
                <th>Module</th>
                <th>Status</th>
                <th>Covers</th>
              </tr>
            </thead>
            <tbody>
              {MODULES.map((m) => (
                <tr key={m.name}>
                  <td>
                    <a href={repo(`blob/main/pyc_lib/${m.name}.py`)}>
                      <code>{m.name}</code>
                    </a>
                  </td>
                  <td>
                    <span className={`pill ${m.status}`}>
                      {m.status === 'ok' ? 'complete' : 'partial'}
                    </span>
                  </td>
                  <td>
                    {m.covers}
                    {m.note && (
                      <>
                        <br />
                        <span className="muted small">{m.note}</span>
                      </>
                    )}
                  </td>
                </tr>
              ))}
            </tbody>
          </table>
        </div>
        <p className="small muted">
          &ldquo;Complete&rdquo; means the commonly used surface of the module is implemented
          with CPython&apos;s behaviour, not that every name in CPython&apos;s module exists.
        </p>

        <h2 id="next">Next up</h2>
        <p>
          These are the modules that real programs reach for most often and that pyc does not
          have yet. Each one is a self-contained contribution.
        </p>
        <div className="grid-2">
          {WANTED.map((w) => (
            <div className="card" key={w.name}>
              <h3>
                <code>{w.name}</code>
              </h3>
              <p>{w.why}</p>
            </div>
          ))}
        </div>

        <h2 id="write">Writing a module</h2>
        <p>
          A module is a Python file in <code>pyc_lib/</code>. Write it in plain Python wherever
          possible. Porting CPython&apos;s own pure-Python implementation is often the right
          start. Where you need the C library, bind a function with{' '}
          <code>__pyc_c_call__</code>:
        </p>
        <Code title="pyc_lib/math.py (excerpt)">{`
pi = 3.141592653589793

def sqrt(x): return __pyc_c_call__(float, "sqrt", float, x)
def floor(x): ...   # returns int, as in CPython 3
`}</Code>
        <p>Then:</p>
        <ol>
          <li>
            Add tests under <code>tests/</code>. The harness compares every test&apos;s output
            with CPython&apos;s, on both backends.
          </li>
          <li>
            Check the module under <code>--strict</code>. It should compile without any
            accommodation, so that strict programs can use it.
          </li>
          <li>
            Run <code>make test</code>.
          </li>
        </ol>
        <p>
          The runtime guide, <a href={repo('blob/main/RUNTIME.md')}>RUNTIME.md</a>, has
          recipes for adding new runtime support.
        </p>
      </DocLayout>
    </>
  );
}
