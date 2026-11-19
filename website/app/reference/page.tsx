import type { Metadata } from 'next';
import Link from 'next/link';
import { Code } from '@/components/Code';
import { DocLayout, PageHero } from '@/components/DocPage';
import { repo } from '@/content/site';

export const metadata: Metadata = {
  title: 'Reference',
  description:
    'pyc command-line options, compilation modes, supported Python 3 features, language extensions and known limitations.',
};

const TOC = [
  { id: 'usage', label: 'Usage' },
  { id: 'options', label: 'Options' },
  { id: 'modes', label: 'Modes' },
  { id: 'imports', label: 'Module search path' },
  { id: 'supported', label: 'Supported Python' },
  { id: 'extensions', label: 'Language extensions' },
  { id: 'limits', label: 'Limitations' },
];

const OPTIONS: [string, string][] = [
  ['-o <path>, --output <path>', 'Output binary path. Default: the input file name with its extension stripped.'],
  ['-x, --exec', 'Compile (cached in ~/.cache/pyc) and run. Arguments after the script go to the program. For #! lines.'],
  ['--strict', 'CPython semantics or a compile error: every type violation is fatal, and no accommodations apply.'],
  ['--permissive', 'The default. Named accommodations are allowed, and each one warns where it applies.'],
  ['--safe', 'Permissive, and also zero-initialize locals that may be read before assignment.'],
  ['--refuse-imprecise', 'Refuse a program whose inference left a container element or a parameter untyped.'],
  ['-b, --emit-llvm', 'Use the LLVM backend.'],
  ['-j, --jit', 'Compile with LLVM and run in-process (implies -b).'],
  ['--repl', 'Interactive REPL, backed by the JIT.'],
  ['-O, --optimize', 'Compile the generated C at -O2 (the default).'],
  ['--no-optimize', 'Compile the generated C at -O0, for debugging.'],
  ['-g, --debug-info', 'Emit debug information.'],
  ['--html', 'Emit an HTML view of the type-annotated program.'],
  ['-D <dir>, --system-directory <dir>', "pyc's library directory. Default: $PYC_SYSTEM_DIRECTORY, else ., else the pyc executable's directory."],
  ['-v, --verbose / -d, --debug', 'More output; repeat for more. --help also lists the development options.'],
];

const SUPPORTED: [string, string][] = [
  ['Classes', 'Inheritance, super(), class attributes, operator overloading, __str__/__repr__, __iter__/__next__, __enter__/__exit__, read-only @property, @staticmethod, @classmethod (cls(...))'],
  ['Functions', 'Closures, nonlocal/global, lambdas, default and mutable default arguments, *args, nested functions, local classes'],
  ['Exceptions', 'try/except/else/finally, raise, user hierarchies, built-in exceptions raised by the runtime'],
  ['Generators', 'yield, yield from, send(), return values'],
  ['async / await', "Compiled to coroutines and run on pyc's own event loop (no asyncio module yet)"],
  ['Control flow', 'if/elif/else, for/while with else, with, match/case (all PEP 634 patterns), comprehensions, generator expressions, starred assignment'],
  ['Built-in types', 'int (64-bit), float, bool, str, bytes, bytearray, list, tuple, dict, set, range, slice, None'],
  ['Strings', 'f-strings, %-formatting, the str method set'],
  ['Modules', 'import, from … import, packages and relative imports, all resolved at compile time'],
];

const EXTENSIONS: [string, string][] = [
  ['__pyc_c_call__(ret, fn, ...)', 'Call a C function inline'],
  ['__pyc_primitive__(sym, ...)', 'Invoke a compiler primitive'],
  ['__pyc_char__', 'The uint8 / C char type'],
  ['__pyc_operator__', 'Access C-level operators directly'],
  ['__pyc_insert_c_header__(h)', 'Include a C header in the generated code'],
  ['@vector("s")', 'Declare a class as a fixed-size value-type vector'],
];

export default function Reference() {
  return (
    <>
      <PageHero title="Reference">
        The command line, the modes, and exactly what subset of Python 3 pyc compiles today.
      </PageHero>
      <DocLayout toc={TOC}>
        <h2 id="usage">Usage</h2>
        <Code lang="shell">{`
$ pyc [options] <file.py>
`}</Code>
        <p>
          pyc reads <code>&lt;file.py&gt;</code> and everything it imports, analyzes the whole
          program, and writes a native executable. The intermediate C is kept as{' '}
          <code>&lt;file.py&gt;.c</code>.
        </p>

        <h2 id="options">Options</h2>
        <div className="table-wrap">
          <table>
            <thead>
              <tr>
                <th>Flag</th>
                <th>Effect</th>
              </tr>
            </thead>
            <tbody>
              {OPTIONS.map(([flag, desc]) => (
                <tr key={flag}>
                  <td>
                    <code>{flag}</code>
                  </td>
                  <td>{desc}</td>
                </tr>
              ))}
            </tbody>
          </table>
        </div>

        <h2 id="modes">Modes</h2>
        <p>
          Static typing without boxing can&apos;t express everything CPython does. pyc&apos;s
          rule is that it never deviates silently.
        </p>
        <ul>
          <li>
            <strong>
              <code>--strict</code>
            </strong>
            : the program has CPython&apos;s semantics, or it does not compile.
          </li>
          <li>
            <strong>
              <code>--permissive</code>
            </strong>{' '}
            (the default) additionally allows a few named accommodations. For example, a
            numeric slot holding both <code>int</code> and <code>float</code> is widened to{' '}
            <code>float</code>. Each accommodation is behind a named flag, warns where it applies,
            and is off under <code>--strict</code>.
          </li>
        </ul>
        <p>
          A value that genuinely holds two unrelated types is refused in every mode, with the
          variable and the union named. More in <Link href="/strict/">Strict dialect</Link>.
        </p>

        <h2 id="imports">Module search path</h2>
        <p>
          Imports resolve against the <strong>current directory</strong>, then{' '}
          <code>&lt;pyc-dir&gt;/pyc_lib/</code>, then each directory in{' '}
          <code>PYTHONPATH</code>. The source file&apos;s own directory is not searched, so
          compile from the program&apos;s directory or add it to <code>PYTHONPATH</code>.
        </p>
        <Code lang="shell">{`
$ PYTHONPATH=/my/libs pyc myprogram.py
`}</Code>

        <h2 id="supported">Supported Python</h2>
        <div className="table-wrap">
          <table>
            <tbody>
              {SUPPORTED.map(([k, v]) => (
                <tr key={k}>
                  <th style={{ width: '9em' }}>{k}</th>
                  <td>{v}</td>
                </tr>
              ))}
            </tbody>
          </table>
        </div>

        <h2 id="extensions">Language extensions</h2>
        <p>
          pyc&apos;s builtin library (<code>__pyc__/</code>) is itself Python, written with a
          few compiler directives. They are available to library authors, and are the way to
          bind a C function:
        </p>
        <div className="table-wrap">
          <table>
            <thead>
              <tr>
                <th>Directive</th>
                <th>Purpose</th>
              </tr>
            </thead>
            <tbody>
              {EXTENSIONS.map(([k, v]) => (
                <tr key={k}>
                  <td>
                    <code>{k}</code>
                  </td>
                  <td>{v}</td>
                </tr>
              ))}
            </tbody>
          </table>
        </div>
        <Code title="pyc_lib/math.py (excerpt)">{`
def sqrt(x): return __pyc_c_call__(float, "sqrt", float, x)
`}</Code>

        <h2 id="limits">Limitations</h2>
        <p>Out of scope by design, because they conflict with whole-program static typing:</p>
        <ul>
          <li>
            <code>eval</code>, <code>exec</code>, dynamic <code>import</code>, and{' '}
            <code>getattr</code>/<code>setattr</code> with run-time names
          </li>
          <li>The full metaclass protocol, and the CPython C-extension API</li>
          <li>Values that need boxing: one slot holding unrelated types over time</li>
        </ul>
        <p>Not yet supported, each tracked by an issue:</p>
        <ul>
          <li>
            <code>**kwargs</code> and keyword-only parameters (
            <a href={repo('blob/main/issues/182-kwargs-and-keyword-only-parameters.md')}>
              issues/182
            </a>
            )
          </li>
          <li>
            Property setters and deleters (
            <a href={repo('blob/main/issues/007-decorators-not-applied.md')}>issues/007</a>)
          </li>
          <li>
            <code>complex</code>
          </li>
          <li>
            Attribute reads through <code>cls</code> in a <code>@classmethod</code> (
            <a href={repo('blob/main/issues/184-classmethod-attribute-through-cls.md')}>
              issues/184
            </a>
            )
          </li>
          <li>
            Integers past 64 bits. Literals that don&apos;t fit are warned, and refused under{' '}
            <code>--strict</code>.
          </li>
          <li>
            The standard library is limited to the modules in <code>pyc_lib/</code>. See{' '}
            <Link href="/libraries/">Libraries</Link>.
          </li>
        </ul>
        <p>
          The full list, including known wrong answers under repair, is in{' '}
          <a href={repo('tree/main/issues')}>issues/</a> and{' '}
          <a href={repo('tree/main/ifa/issues')}>ifa/issues/</a>.
        </p>
      </DocLayout>
    </>
  );
}
