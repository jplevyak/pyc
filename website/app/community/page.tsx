import type { Metadata } from 'next';
import Link from 'next/link';
import { Code } from '@/components/Code';
import { DocLayout, PageHero } from '@/components/DocPage';
import { GITHUB, repo } from '@/content/site';

export const metadata: Metadata = {
  title: 'Community',
  description: 'Report a bug, add a library module, or work on the compiler.',
};

const TOC = [
  { id: 'start', label: 'Where to start' },
  { id: 'bugs', label: 'Reporting a bug' },
  { id: 'rules', label: 'Design rules' },
  { id: 'accept', label: 'Change acceptance' },
  { id: 'docs', label: 'Further reading' },
];

const RULES: [string, string][] = [
  [
    'CPython semantics, not another compiler’s.',
    'If a change makes pyc agree with something else and disagree with CPython, it is wrong.',
  ],
  [
    'Split only on demand.',
    'A function or data contour exists because something observed a distinction that required it, never because a neighbouring contour split.',
  ],
  [
    'Provenance is never identity.',
    'Contours are keyed on what the types are, never on where a value came from.',
  ],
  [
    'Never decide by name.',
    'Analysis and code generation use the call graph and the class hierarchy, never matching identifier strings.',
  ],
  [
    'Root-cause; don’t retreat.',
    'A conservative fallback that makes a test pass needs proof that the better solution can’t be reached.',
  ],
  [
    'Every accommodation is flagged.',
    'Anything beyond CPython 3 semantics is named, warns, and is off under --strict.',
  ],
];

export default function Community() {
  return (
    <>
      <PageHero title="Community">
        pyc is open source under the BSD 3-Clause license, developed in the open on GitHub.
      </PageHero>
      <DocLayout toc={TOC}>
        <h2 id="start">Where to start</h2>
        <div className="grid-2">
          <div className="card">
            <h3>Add a library module</h3>
            <p>
              The most self-contained way to contribute. Pick a module from the{' '}
              <Link href="/libraries/#next">wanted list</Link>, write it in Python, and test it
              against CPython.
            </p>
          </div>
          <div className="card">
            <h3>Compile your program</h3>
            <p>
              Try pyc on real code. Each refusal is either a real limit, which belongs in the
              documentation, or a pyc bug, which belongs in an issue.
            </p>
          </div>
          <div className="card">
            <h3>Fix a filed issue</h3>
            <p>
              Each issue documents the symptom, the root cause where known, a proposed fix and
              a verification plan.
            </p>
            <p>
              <a href={repo('tree/main/issues')}>Browse issues →</a>
            </p>
          </div>
          <div className="card">
            <h3>Work on the analysis</h3>
            <p>
              The IFA library is a general interprocedural analysis with extensive design notes.
              Read the design rules below before changing it.
            </p>
            <p>
              <a href={repo('blob/main/ifa/INDEX.md')}>ifa/INDEX.md →</a>
            </p>
          </div>
        </div>

        <h2 id="bugs">Reporting a bug</h2>
        <p>The most useful report is a short program plus both outputs:</p>
        <Code lang="shell">{`
$ python3 repro.py > expected.txt
$ pyc --strict repro.py && ./repro > actual.txt
$ diff expected.txt actual.txt
`}</Code>
        <p>
          A program that compiles and prints something different from CPython is the most
          serious kind of bug pyc can have. Please say so in the report title. Open issues on{' '}
          <a href={`${GITHUB}/issues`}>GitHub</a>.
        </p>

        <h2 id="rules">Design rules</h2>
        <p>
          pyc has a short list of rules that every change is measured against. They are
          spelled out, with the history behind each one, in{' '}
          <a href={repo('blob/main/AGENTS.md')}>AGENTS.md</a>.
        </p>
        <div className="table-wrap">
          <table>
            <tbody>
              {RULES.map(([k, v]) => (
                <tr key={k}>
                  <th style={{ whiteSpace: 'normal', width: '15em' }}>{k}</th>
                  <td>{v}</td>
                </tr>
              ))}
            </tbody>
          </table>
        </div>

        <h2 id="accept">Change acceptance</h2>
        <p>One command runs everything CI runs, on both backends:</p>
        <Code lang="shell">{`
$ make test
`}</Code>
        <p>
          For a change to the analysis, the 77-program corpus is the larger benchmark.{' '}
          <code>./corpus_sweep.sh -m check</code> compiles and runs every program and compares
          its output with CPython&apos;s. Results are cached under <code>sweeps/</code>, keyed on
          the source tree.
        </p>

        <h2 id="docs">Further reading</h2>
        <ul>
          <li>
            <a href={repo('blob/main/PIPELINE.md')}>PIPELINE.md</a>: from{' '}
            <code>pyc file.py</code> to a binary, phase by phase
          </li>
          <li>
            <a href={repo('blob/main/PYTHON_FRONTEND.md')}>PYTHON_FRONTEND.md</a>: the Python
            lowering
          </li>
          <li>
            <a href={repo('blob/main/RUNTIME.md')}>RUNTIME.md</a>: the C runtime and the
            builtin library
          </li>
          <li>
            <a href={repo('blob/main/ifa/INDEX.md')}>ifa/INDEX.md</a>: the analysis library
          </li>
        </ul>
      </DocLayout>
    </>
  );
}
