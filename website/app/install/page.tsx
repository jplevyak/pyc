import type { Metadata } from 'next';
import Link from 'next/link';
import { Code } from '@/components/Code';
import { Callout, DocLayout, PageHero } from '@/components/DocPage';
import { DPARSER, GITHUB } from '@/content/site';

export const metadata: Metadata = {
  title: 'Install',
  description: 'Build pyc from source on Linux or macOS and compile your first program.',
};

const TOC = [
  { id: 'requirements', label: 'Requirements' },
  { id: 'build', label: 'Build from source' },
  { id: 'path', label: 'Put pyc on your PATH' },
  { id: 'first', label: 'Your first program' },
  { id: 'scripts', label: 'Run scripts with #!' },
  { id: 'repl', label: 'JIT and REPL' },
  { id: 'verify', label: 'Verify the build' },
];

export default function Install() {
  return (
    <>
      <PageHero title="Install pyc">
        pyc is built from source. It needs clang, LLVM, the Boehm garbage collector and DParser.
        A full build takes a few minutes.
      </PageHero>
      <DocLayout toc={TOC}>
        <h2 id="requirements">Requirements</h2>
        <ul>
          <li>
            <strong>clang / clang++</strong> with C++23 support, and <code>llvm-ar</code>
          </li>
          <li>
            <strong>LLVM</strong> development libraries, with <code>llvm-config</code> on your{' '}
            <code>PATH</code>. The LLVM backend is always built.
          </li>
          <li>
            <strong>Boehm GC</strong> (<code>libgc</code>, <code>libgccpp</code>)
          </li>
          <li>
            <strong>PCRE</strong> (<code>libpcre</code>)
          </li>
          <li>
            <strong>
              <a href={DPARSER}>DParser</a>
            </strong>
            , built with GC support and installed
          </li>
        </ul>
        <Code lang="shell" title="Ubuntu / Debian">{`
$ sudo apt-get install clang-20 llvm-20-dev libgc-dev libpcre3-dev
`}</Code>
        <Code lang="shell" title="Fedora / RHEL">{`
$ sudo dnf install clang llvm-devel gc-devel pcre-devel
`}</Code>
        <Callout title="Use LLVM 20.">
          <p>
            CI runs clang/LLVM 20 on Ubuntu 24.04. The unversioned packages there are LLVM 18,
            whose coroutine ABI breaks the generator and async tests.
          </p>
        </Callout>

        <h2 id="build">Build from source</h2>
        <p>
          The IFA analysis library ships inside the repository under <code>ifa/</code> and is
          built automatically. Install DParser first:
        </p>
        <Code lang="shell">{`
# 1. DParser, with garbage-collector support
$ git clone ${DPARSER}.git
$ cd dparser && sudo make install D_USE_GC=1 && cd ..

# 2. pyc
$ git clone ${GITHUB}.git
$ cd pyc
$ make
`}</Code>
        <p>
          <code>make</code> builds <code>ifa/libifa_gc.a</code>, the runtime, and the{' '}
          <code>pyc</code> binary. pyc itself is built at <code>-O3 -march=native</code> by
          default. For a debuggable compiler, use <code>make clean &amp;&amp; make OPTIMIZE=</code>.
        </p>
        <Callout title="After changing a header or a build flag, run make clean.">
          <p>
            The Makefiles track neither, and stale objects produce failures that look like real
            bugs.
          </p>
        </Callout>

        <h2 id="path">Put pyc on your PATH</h2>
        <p>
          pyc finds its builtin library (<code>__pyc__/</code>, <code>pyc_lib/</code> and the
          runtime) next to its own executable, following symlinks. A symlink from the checkout
          is enough:
        </p>
        <Code lang="shell">{`
$ ln -s "$PWD/pyc" ~/.local/bin/pyc
`}</Code>
        <p>
          You can also name the directory explicitly with <code>-D &lt;dir&gt;</code> or{' '}
          <code>$PYC_SYSTEM_DIRECTORY</code>.
        </p>

        <h2 id="first">Your first program</h2>
        <Code title="hello.py">{`
def fib(n):
    return n if n < 2 else fib(n - 1) + fib(n - 2)

print(fib(35))
`}</Code>
        <Code lang="shell">{`
$ pyc hello.py          # writes ./hello (and keeps hello.py.c)
$ ./hello
9227465
`}</Code>
        <p>
          pyc reads the file and everything it imports, analyzes the whole program, and writes
          an executable named after the input. Use <code>-o</code> to choose the name. Imports
          resolve against the current directory, then <code>pyc_lib/</code>, then{' '}
          <code>$PYTHONPATH</code>.
        </p>

        <h2 id="scripts">Run scripts with #!</h2>
        <p>
          <code>-x</code> compiles once, caches the binary under <code>~/.cache/pyc</code>, and
          runs it. A later run whose sources are all unchanged starts the cached executable in a
          few milliseconds.
        </p>
        <Code title="greet.py">{`
#!/usr/bin/env -S pyc -x
import sys
print("hello", sys.argv[1:])
`}</Code>
        <Code lang="shell">{`
$ chmod +x greet.py
$ ./greet.py a b
hello ['a', 'b']
`}</Code>

        <h2 id="repl">JIT and REPL</h2>
        <Code lang="shell">{`
$ pyc -j program.py     # compile with LLVM and run in-process
$ pyc --repl            # interactive, backed by the JIT
`}</Code>

        <h2 id="verify">Verify the build</h2>
        <p>
          <code>make test</code> runs everything CI runs, on both backends, in about four
          minutes. When it passes, every end-to-end test matched both its expected output and
          CPython.
        </p>
        <Code lang="shell">{`
$ make test
`}</Code>
        <p>
          Next: <Link href="/learn/">take the tour</Link>.
        </p>
      </DocLayout>
    </>
  );
}
